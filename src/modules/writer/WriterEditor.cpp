#include "modules/writer/WriterEditor.h"

#include "core/ActionManager.h"
#include "core/AppSettings.h"
#include "core/Icons.h"
#include "core/ModuleRegistry.h"
#include "core/TextOps.h"
#include "core/ThemeManager.h"
#include "core/widgets/ColorButton.h"
#include "core/widgets/FindReplaceBar.h"
#include "core/widgets/Ribbon.h"
#include "formats/Docx.h"
#include "formats/TextCodec.h"
#include "modules/writer/DocumentPrinter.h"
#include "modules/writer/PageView.h"
#include "modules/writer/WriterDialogs.h"
#include "modules/writer/WriterTemplates.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPrinter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextDocumentFragment>
#include <QTextDocumentWriter>
#include <QTextList>
#include <QTextTable>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {

const QList<qreal> kFontSizes = {8, 9, 10, 10.5, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72};

bool isImageFile(const QString &path)
{
    const QByteArray suffix = QFileInfo(path).suffix().toLower().toLatin1();
    return QImageReader::supportedImageFormats().contains(suffix) || suffix == "jpg";
}

QString saveImagesBesides(QTextDocument *doc, const QString &targetPath, QString html, bool markdown)
{
    // Collect image resources and replace their names with files written next to the document.
    const QFileInfo info(targetPath);
    const QString folderName = info.completeBaseName() + QStringLiteral("_resimler");
    int counter = 0;
    QHash<QString, QString> done;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat())
                continue;
            const QString name = frag.charFormat().toImageFormat().name();
            if (done.contains(name) || name.startsWith(QLatin1String("http")))
                continue;
            const QVariant res = doc->resource(QTextDocument::ImageResource, QUrl(name));
            QImage image = res.value<QImage>();
            if (image.isNull() && res.canConvert<QPixmap>())
                image = res.value<QPixmap>().toImage();
            if (image.isNull())
                continue;
            QString replacement;
            if (markdown) {
                QDir().mkpath(info.absoluteDir().filePath(folderName));
                const QString file = QStringLiteral("resim%1.png").arg(++counter);
                image.save(info.absoluteDir().filePath(folderName + QLatin1Char('/') + file), "PNG");
                replacement = folderName + QLatin1Char('/') + file;
            } else {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, "PNG");
                replacement = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
            }
            done.insert(name, replacement);
        }
    }
    for (auto it = done.cbegin(); it != done.cend(); ++it) {
        if (markdown)
            html.replace(QLatin1Char('(') + it.key() + QLatin1Char(')'), QLatin1Char('(') + it.value() + QLatin1Char(')'));
        else
            html.replace(QStringLiteral("src=\"") + it.key() + QLatin1Char('"'), QStringLiteral("src=\"") + it.value() + QLatin1Char('"'));
    }
    return html;
}

bool writeText(const QString &path, const QString &text, QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("WriterEditor", "Dosya yazılamadı: %1").arg(file.errorString());
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------------
// WriterDocument

QVariant WriterDocument::loadResource(int type, const QUrl &name)
{
    if (type == QTextDocument::ImageResource) {
        const QString s = name.toString();
        if (s.startsWith(QLatin1String("data:"), Qt::CaseInsensitive)) {
            const int comma = s.indexOf(QLatin1Char(','));
            if (comma > 0) {
                const QByteArray payload = s.mid(comma + 1).toLatin1();
                const QByteArray raw = s.left(comma).contains(QLatin1String("base64"), Qt::CaseInsensitive)
                                           ? QByteArray::fromBase64(payload)
                                           : QByteArray::fromPercentEncoding(payload);
                QImage image;
                if (image.loadFromData(raw))
                    return image;
            }
        }
        QString path = name.isLocalFile() ? name.toLocalFile() : QUrl::fromPercentEncoding(s.toUtf8());
        if (QFileInfo(path).isRelative() && !baseDirectory.isEmpty())
            path = QDir(baseDirectory).filePath(path);
        QImage image;
        if (QFileInfo::exists(path) && image.load(path))
            return image;
    }
    return QTextDocument::loadResource(type, name);
}

// ---------------------------------------------------------------------------------
// WriterEditor

WriterEditor::WriterEditor(Module *module, QWidget *parent)
    : DocumentEditor(module, parent)
{
    m_doc = new WriterDocument(this);
    TextOps::setupDocument(m_doc);

    m_view = new PageView(this);
    m_view->setDocument(m_doc);
    m_view->setPageSetup(m_setup);

    m_find = new FindReplaceBar(this);

    m_outlinePanel = new QWidget(this);
    m_outlinePanel->setObjectName(QStringLiteral("SidePanel"));
    m_outlinePanel->setAttribute(Qt::WA_StyledBackground, true);
    auto *outlineLayout = new QVBoxLayout(m_outlinePanel);
    outlineLayout->setContentsMargins(0, 0, 0, 0);
    outlineLayout->setSpacing(0);
    auto *outlineTitle = new QLabel(tr("Gezinti"), m_outlinePanel);
    outlineTitle->setObjectName(QStringLiteral("PanelTitle"));
    m_outline = new QTreeWidget(m_outlinePanel);
    m_outline->setHeaderHidden(true);
    m_outline->setFrameShape(QFrame::NoFrame);
    m_outline->setIndentation(14);
    outlineLayout->addWidget(outlineTitle);
    outlineLayout->addWidget(m_outline, 1);
    m_outlinePanel->setMinimumWidth(180);
    m_outlinePanel->hide();

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_outlinePanel);
    m_splitter->addWidget(m_view);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({240, 900});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_find);
    layout->addWidget(m_splitter, 1);
    setFocusProxy(m_view);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(250);
    connect(m_statusTimer, &QTimer::timeout, this, &WriterEditor::updateStatus);
    m_outlineTimer = new QTimer(this);
    m_outlineTimer->setSingleShot(true);
    m_outlineTimer->setInterval(600);
    connect(m_outlineTimer, &QTimer::timeout, this, &WriterEditor::rebuildOutline);
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(180);
    connect(m_searchTimer, &QTimer::timeout, this, &WriterEditor::runSearch);

    createActions();
    connectView();
    updateFormatState();
}

WriterEditor::~WriterEditor() = default;

QTextCursor WriterEditor::textCursor() const
{
    return m_view->textCursor();
}

void WriterEditor::applyCursor(const QTextCursor &cursor)
{
    m_view->setTextCursor(cursor);
    m_view->focusText();
    m_view->ensureCursorVisible();
    updateFormatState();
}

qreal WriterEditor::textWidth() const
{
    const QMarginsF m = m_setup.marginsPx();
    return m_setup.sizePx().width() - m.left() - m.right();
}

void WriterEditor::createActions()
{
    auto create = [this](const char *id) {
        QAction *a = ActionManager::instance().create(QString::fromLatin1(id), this);
        m_actions.insert(QString::fromLatin1(id), a);
        return a;
    };
    auto onTrigger = [this, create](const char *id, auto slot) {
        QAction *a = create(id);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    onTrigger("writer.paste", [this] { paste(false); });
    onTrigger("writer.pastePlain", [this] { paste(true); });
    onTrigger("writer.cut", [this] { copySelection(true); });
    onTrigger("writer.copy", [this] { copySelection(false); });
    QAction *painter = create("writer.formatPainter");
    connect(painter, &QAction::toggled, this, [this](bool on) {
        m_painterActive = on;
        if (on)
            m_painterFormat = textCursor().charFormat();
        showMessage(on ? tr("Biçimi uygulamak için metin seçin.") : QString());
    });

    onTrigger("writer.bold", [this](bool on) {
        QTextCharFormat f;
        f.setFontWeight(on ? QFont::Bold : QFont::Normal);
        mergeFormat(f);
    });
    onTrigger("writer.italic", [this](bool on) {
        QTextCharFormat f;
        f.setFontItalic(on);
        mergeFormat(f);
    });
    onTrigger("writer.underline", [this](bool on) {
        QTextCharFormat f;
        f.setFontUnderline(on);
        mergeFormat(f);
    });
    onTrigger("writer.strike", [this](bool on) {
        QTextCharFormat f;
        f.setFontStrikeOut(on);
        mergeFormat(f);
    });
    onTrigger("writer.subscript", [this](bool on) {
        QTextCharFormat f;
        f.setVerticalAlignment(on ? QTextCharFormat::AlignSubScript : QTextCharFormat::AlignNormal);
        mergeFormat(f);
    });
    onTrigger("writer.superscript", [this](bool on) {
        QTextCharFormat f;
        f.setVerticalAlignment(on ? QTextCharFormat::AlignSuperScript : QTextCharFormat::AlignNormal);
        mergeFormat(f);
    });
    onTrigger("writer.growFont", [this] { growFont(1); });
    onTrigger("writer.shrinkFont", [this] { growFont(-1); });
    onTrigger("writer.clearFormat", [this] {
        QTextCursor c = textCursor();
        TextOps::clearFormatting(c);
        applyCursor(c);
    });
    onTrigger("writer.changeCase", [this] { changeCaseCycle(); });

    auto align = [this](Qt::Alignment a) {
        QTextCursor c = textCursor();
        TextOps::setAlignment(c, a);
        applyCursor(c);
    };
    onTrigger("writer.alignLeft", [align] { align(Qt::AlignLeft | Qt::AlignAbsolute); });
    onTrigger("writer.alignCenter", [align] { align(Qt::AlignHCenter); });
    onTrigger("writer.alignRight", [align] { align(Qt::AlignRight | Qt::AlignAbsolute); });
    onTrigger("writer.alignJustify", [align] { align(Qt::AlignJustify); });

    onTrigger("writer.bullets", [this] {
        QTextCursor c = textCursor();
        TextOps::toggleList(c, QTextListFormat::ListDisc);
        applyCursor(c);
    });
    onTrigger("writer.numbering", [this] {
        QTextCursor c = textCursor();
        TextOps::toggleList(c, QTextListFormat::ListDecimal);
        applyCursor(c);
    });
    onTrigger("writer.checklist", [this] {
        QTextCursor c = textCursor();
        TextOps::toggleChecklist(c);
        applyCursor(c);
    });
    onTrigger("writer.indentLess", [this] {
        QTextCursor c = textCursor();
        TextOps::changeIndent(c, -1);
        applyCursor(c);
    });
    onTrigger("writer.indentMore", [this] {
        QTextCursor c = textCursor();
        TextOps::changeIndent(c, 1);
        applyCursor(c);
    });
    onTrigger("writer.showMarks", [this](bool on) {
        QTextOption option = m_doc->defaultTextOption();
        QTextOption::Flags flags = option.flags();
        flags.setFlag(QTextOption::ShowTabsAndSpaces, on);
        flags.setFlag(QTextOption::ShowLineAndParagraphSeparators, on);
        option.setFlags(flags);
        m_doc->setDefaultTextOption(option);
    });
    create("writer.lineSpacing");

    onTrigger("writer.find", [this] { showFind(); });
    onTrigger("writer.replace", [this] { showReplace(); });
    onTrigger("writer.selectAll", [this] {
        QTextCursor c = textCursor();
        c.select(QTextCursor::Document);
        applyCursor(c);
    });

    onTrigger("writer.pageBreak", [this] {
        QTextCursor c = textCursor();
        TextOps::insertPageBreak(c);
        applyCursor(c);
    });
    create("writer.table");
    onTrigger("writer.image", [this] { insertImagesFromDialog(); });
    onTrigger("writer.link", [this] { insertLink(); });
    onTrigger("writer.removeLink", [this] {
        QTextCursor c = textCursor();
        TextOps::removeHyperlink(c);
        applyCursor(c);
    });
    onTrigger("writer.hr", [this] {
        QTextCursor c = textCursor();
        TextOps::insertHorizontalRule(c);
        applyCursor(c);
    });
    onTrigger("writer.dateTime", [this] { insertDateTime(); });
    create("writer.symbol");
    onTrigger("writer.toc", [this] { insertTableOfContents(); });

    for (const char *id : {"writer.rowAbove", "writer.rowBelow", "writer.colLeft", "writer.colRight", "writer.deleteRow",
                           "writer.deleteColumn", "writer.deleteTable", "writer.mergeCells", "writer.splitCell",
                           "writer.tableBorders"}) {
        const QString command = QString::fromLatin1(id);
        onTrigger(id, [this, command] { tableCommand(command); });
    }

    create("writer.margins");
    create("writer.orientation");
    create("writer.pageSize");
    onTrigger("writer.pageSetup", [this] { showPageSetupDialog(); });
    onTrigger("writer.headerFooter", [this] { showPageSetupDialog(); });
    onTrigger("writer.pageNumbers", [this](bool on) {
        PageSetup s = m_setup;
        s.footer = on ? tr("Sayfa {page} / {pages}") : QString();
        applyPageSetup(s);
    });
    onTrigger("writer.wordCount", [this] { showWordCount(); });
    onTrigger("writer.sendMail", [this] { sendByMail(); });
    onTrigger("writer.navPane", [this](bool on) {
        m_outlinePanel->setVisible(on);
        if (on)
            rebuildOutline();
    });
    onTrigger("writer.darkPage", [this](bool on) { m_view->setDarkPage(on); });
    onTrigger("writer.fitWidth", [this] { m_view->setFitMode(PageView::FitMode::Width); });
    onTrigger("writer.onePage", [this] { m_view->setFitMode(PageView::FitMode::Page); });
    onTrigger("writer.zoom100", [this] { m_view->setZoomPercent(100); });

    // Alignment buttons behave like a radio group.
    auto *alignGroup = new QActionGroup(this);
    for (const char *id : {"writer.alignLeft", "writer.alignCenter", "writer.alignRight", "writer.alignJustify"})
        alignGroup->addAction(m_actions.value(QString::fromLatin1(id)));
}

void WriterEditor::connectView()
{
    PageTextItem *item = m_view->textItem();
    item->keyFilter = [this](QKeyEvent *e) { return handleKey(e); };
    connect(item, &PageTextItem::cursorMoved, this, [this] {
        updateFormatState();
        m_view->ensureCursorVisible();
        scheduleStatus();
    });
    connect(item, &PageTextItem::enterPressed, this, [this] {
        QTextCursor c = textCursor();
        TextOps::afterNewParagraph(c);
        m_view->setTextCursor(c);
        updateFormatState();
    });
    connect(item, &PageTextItem::pasteRequested, this, [this] { paste(false); });
    connect(item, &PageTextItem::contextMenuRequested, this, &WriterEditor::showContextMenu);
    connect(item, &PageTextItem::linkClicked, this, &WriterEditor::openLink);
    connect(item, &PageTextItem::imageDoubleClicked, this, &WriterEditor::editImage);
    connect(item, &PageTextItem::filesDropped, this, &WriterEditor::insertImageFiles);
    connect(item, &PageTextItem::mouseReleased, this, [this] {
        if (!m_painterActive)
            return;
        QTextCursor c = textCursor();
        if (!c.hasSelection())
            return;
        c.mergeCharFormat(m_painterFormat);
        m_actions.value(QStringLiteral("writer.formatPainter"))->setChecked(false);
        applyCursor(c);
    });

    connect(m_doc, &QTextDocument::modificationChanged, this, [this](bool modified) { setModified(modified); });
    connect(m_doc, &QTextDocument::undoAvailable, this, &DocumentEditor::undoRedoChanged);
    connect(m_doc, &QTextDocument::redoAvailable, this, &DocumentEditor::undoRedoChanged);
    connect(m_doc, &QTextDocument::contentsChanged, this, [this] {
        scheduleStatus();
        if (m_outlinePanel->isVisible())
            m_outlineTimer->start();
        if (m_find->isVisible())
            m_searchTimer->start();
    });
    connect(m_view, &PageView::zoomChanged, this, &DocumentEditor::zoomChanged);
    connect(m_view, &PageView::pageCountChanged, this, [this] { scheduleStatus(); });

    connect(m_find, &FindReplaceBar::searchChanged, this, [this] { m_searchTimer->start(); });
    connect(m_find, &FindReplaceBar::findNext, this, [this] { findStep(true); });
    connect(m_find, &FindReplaceBar::findPrevious, this, [this] { findStep(false); });
    connect(m_find, &FindReplaceBar::replaceOne, this, &WriterEditor::replaceCurrent);
    connect(m_find, &FindReplaceBar::replaceAll, this, &WriterEditor::replaceEverything);
    connect(m_find, &FindReplaceBar::closed, this, [this] {
        m_matches.clear();
        m_currentMatch = -1;
        updateHighlights();
        m_view->focusText();
    });

    connect(m_outline, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const int position = item->data(0, Qt::UserRole).toInt();
        QTextCursor c(m_doc);
        c.setPosition(qBound(0, position, m_doc->characterCount() - 1));
        m_view->setTextCursor(c);
        m_view->scrollToPosition(position);
        m_view->focusText();
    });
}

// ---------------------------------------------------------------------------------
// Ribbon

QList<RibbonPage *> WriterEditor::createRibbonPages()
{
    auto a = [this](const char *id) { return m_actions.value(QString::fromLatin1(id)); };
    QList<RibbonPage *> pages;

    // Home ---------------------------------------------------------------------------
    auto *home = new RibbonPage(tr("Giriş"));
    {
        RibbonGroup *clip = home->addGroup(tr("Pano"));
        auto *pasteMenu = new QMenu(home);
        pasteMenu->addAction(a("writer.paste"));
        pasteMenu->addAction(a("writer.pastePlain"));
        clip->addLargeAction(a("writer.paste"), pasteMenu, true);
        clip->addSmallAction(a("writer.cut"));
        clip->addSmallAction(a("writer.copy"));
        clip->addSmallAction(a("writer.formatPainter"));

        RibbonGroup *font = home->addGroup(tr("Yazı Tipi"));
        m_fontCombo = new QFontComboBox;
        m_fontCombo->setFixedWidth(150);
        m_fontCombo->setFocusPolicy(Qt::ClickFocus);
        connect(m_fontCombo, &QFontComboBox::textActivated, this, [this](const QString &family) {
            QTextCharFormat f;
            f.setFontFamilies(QStringList{family});
            mergeFormat(f);
        });
        m_sizeCombo = new QComboBox;
        m_sizeCombo->setEditable(true);
        m_sizeCombo->setFixedWidth(58);
        m_sizeCombo->setFocusPolicy(Qt::ClickFocus);
        for (qreal s : kFontSizes)
            m_sizeCombo->addItem(QLocale().toString(s));
        connect(m_sizeCombo, &QComboBox::textActivated, this, [this](const QString &text) {
            bool ok = false;
            qreal size = QLocale().toDouble(text, &ok);
            if (!ok)
                size = text.toDouble(&ok);
            if (ok && size >= 1 && size <= 1638) {
                QTextCharFormat f;
                f.setFontPointSize(size);
                mergeFormat(f);
            }
        });
        auto *caseMenu = new QMenu(home);
        const QList<QPair<QString, TextOps::CaseMode>> cases = {
            {tr("Cümle düzeni."), TextOps::CaseMode::Sentence},
            {tr("küçük harf"), TextOps::CaseMode::Lower},
            {tr("BÜYÜK HARF"), TextOps::CaseMode::Upper},
            {tr("Her Sözcüğün İlk Harfi Büyük"), TextOps::CaseMode::Capitalize},
            {tr("bÜYÜK/kÜÇÜK hARF dEĞİŞTİR"), TextOps::CaseMode::Toggle}};
        for (const auto &entry : cases) {
            const TextOps::CaseMode mode = entry.second;
            caseMenu->addAction(entry.first, this, [this, mode] {
                QTextCursor c = textCursor();
                TextOps::changeCase(c, mode);
                applyCursor(c);
            });
        }
        font->addRow({m_fontCombo, m_sizeCombo, RibbonGroup::iconButton(a("writer.growFont")),
                      RibbonGroup::iconButton(a("writer.shrinkFont")),
                      RibbonGroup::iconButton(a("writer.changeCase"), caseMenu, true),
                      RibbonGroup::iconButton(a("writer.clearFormat"))});
        m_fontColor = new ColorButton(QStringLiteral("font-color"));
        m_fontColor->setToolTip(tr("Yazı Tipi Rengi"));
        m_fontColor->setNoneText(tr("Otomatik"));
        connect(m_fontColor, &ColorButton::colorPicked, this, [this](const QColor &c) {
            if (c.isValid()) {
                QTextCharFormat f;
                f.setForeground(c);
                mergeFormat(f);
            } else {
                clearProperty(QTextFormat::ForegroundBrush);
            }
        });
        m_highlight = new ColorButton(QStringLiteral("highlight"), ColorButton::Palette::Highlight);
        m_highlight->setToolTip(tr("Metin Vurgu Rengi"));
        m_highlight->setNoneText(tr("Renk yok"));
        connect(m_highlight, &ColorButton::colorPicked, this, [this](const QColor &c) {
            if (c.isValid()) {
                QTextCharFormat f;
                f.setBackground(c);
                mergeFormat(f);
            } else {
                clearProperty(QTextFormat::BackgroundBrush);
            }
        });
        font->addRow({RibbonGroup::iconButton(a("writer.bold")), RibbonGroup::iconButton(a("writer.italic")),
                      RibbonGroup::iconButton(a("writer.underline")), RibbonGroup::iconButton(a("writer.strike")),
                      RibbonGroup::iconButton(a("writer.subscript")), RibbonGroup::iconButton(a("writer.superscript")),
                      m_highlight, m_fontColor});

        RibbonGroup *para = home->addGroup(tr("Paragraf"));
        auto *spacingMenu = new QMenu(home);
        auto *spacingGroup = new QActionGroup(spacingMenu);
        for (const int percent : {100, 115, 150, 200, 250, 300}) {
            QAction *sp = spacingMenu->addAction(QLocale().toString(percent / 100.0, 'f', percent % 100 == 0 ? 1 : 2));
            sp->setCheckable(true);
            spacingGroup->addAction(sp);
            connect(sp, &QAction::triggered, this, [this, percent] {
                QTextCursor c = textCursor();
                TextOps::setLineSpacing(c, percent);
                applyCursor(c);
            });
        }
        spacingMenu->addSeparator();
        spacingMenu->addAction(tr("Paragraftan önce boşluk ekle / kaldır"), this, [this] {
            QTextCursor c = textCursor();
            TextOps::setParagraphSpacing(c, c.blockFormat().topMargin() > 0 ? 0 : 12, -1);
            applyCursor(c);
        });
        spacingMenu->addAction(tr("Paragraftan sonra boşluk ekle / kaldır"), this, [this] {
            QTextCursor c = textCursor();
            TextOps::setParagraphSpacing(c, -1, c.blockFormat().bottomMargin() > 0 ? 0 : 8);
            applyCursor(c);
        });
        para->addRow({RibbonGroup::iconButton(a("writer.bullets")), RibbonGroup::iconButton(a("writer.numbering")),
                      RibbonGroup::iconButton(a("writer.checklist")), RibbonGroup::iconButton(a("writer.indentLess")),
                      RibbonGroup::iconButton(a("writer.indentMore")), RibbonGroup::iconButton(a("writer.showMarks"))});
        para->addRow({RibbonGroup::iconButton(a("writer.alignLeft")), RibbonGroup::iconButton(a("writer.alignCenter")),
                      RibbonGroup::iconButton(a("writer.alignRight")), RibbonGroup::iconButton(a("writer.alignJustify")),
                      RibbonGroup::iconButton(a("writer.lineSpacing"), spacingMenu, false)});

        RibbonGroup *styles = home->addGroup(tr("Stiller"));
        auto *gallery = new QWidget;
        auto *grid = new QGridLayout(gallery);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(3);
        int index = 0;
        for (const TextOps::ParagraphStyle &style : TextOps::paragraphStyles()) {
            auto *button = new QToolButton(gallery);
            button->setObjectName(QStringLiteral("StyleChip"));
            button->setText(style.name);
            button->setCheckable(true);
            button->setAutoRaise(true);
            button->setFocusPolicy(Qt::NoFocus);
            button->setFixedSize(108, 26);
            button->setProperty("styleId", style.id);
            button->setProperty("styleColor", style.color);
            QFont f = button->font();
            if (!style.family.isEmpty())
                f.setFamily(style.family);
            f.setBold(style.bold || style.headingLevel > 0);
            f.setItalic(style.italic);
            f.setPointSizeF(style.id == QLatin1String("Title") ? f.pointSizeF() + 2.5
                            : style.headingLevel == 1            ? f.pointSizeF() + 1.5
                                                                 : f.pointSizeF());
            button->setFont(f);
            const QString id = style.id;
            connect(button, &QToolButton::clicked, this, [this, id] {
                QTextCursor c = textCursor();
                TextOps::applyParagraphStyle(c, id);
                applyCursor(c);
            });
            grid->addWidget(button, index / 4, index % 4);
            m_styleButtons << button;
            ++index;
        }
        styles->addLargeWidget(gallery);
        // Style colours are adjusted for the theme so that headings stay readable in dark mode.
        auto recolor = [this] {
            for (QToolButton *b : std::as_const(m_styleButtons)) {
                const QColor c = b->property("styleColor").value<QColor>();
                b->setStyleSheet(c.isValid() ? QStringLiteral("color: %1;").arg(ThemeManager::instance().accentFor(c).name())
                                             : QString());
            }
        };
        recolor();
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, recolor);

        RibbonGroup *editing = home->addGroup(tr("Düzenleme"));
        editing->addSmallAction(a("writer.find"));
        editing->addSmallAction(a("writer.replace"));
        editing->addSmallAction(a("writer.selectAll"));
    }
    pages << home;

    // Insert -------------------------------------------------------------------------
    auto *insert = new RibbonPage(tr("Ekle"));
    {
        RibbonGroup *pagesGroup = insert->addGroup(tr("Sayfalar"));
        pagesGroup->addLargeAction(a("writer.pageBreak"));

        RibbonGroup *tables = insert->addGroup(tr("Tablolar"));
        auto *tableMenu = new QMenu(insert);
        auto *picker = new TableSizePicker(tableMenu);
        auto *pickerAction = new QWidgetAction(tableMenu);
        pickerAction->setDefaultWidget(picker);
        tableMenu->addAction(pickerAction);
        tableMenu->addSeparator();
        tableMenu->addAction(Icons::icon(QStringLiteral("table")), tr("Tablo Ekle..."), this, [this] {
            bool ok = false;
            const int columns = QInputDialog::getInt(this, tr("Tablo Ekle"), tr("Sütun sayısı:"), 3, 1, 63, 1, &ok);
            if (!ok)
                return;
            const int rows = QInputDialog::getInt(this, tr("Tablo Ekle"), tr("Satır sayısı:"), 3, 1, 500, 1, &ok);
            if (!ok)
                return;
            QTextCursor c = textCursor();
            TextOps::insertTable(c, rows, columns);
            applyCursor(c);
        });
        connect(picker, &TableSizePicker::picked, this, [this, tableMenu](int rows, int columns) {
            tableMenu->close();
            QTextCursor c = textCursor();
            TextOps::insertTable(c, rows, columns);
            applyCursor(c);
        });
        tables->addLargeAction(a("writer.table"), tableMenu, false);

        RibbonGroup *illustrations = insert->addGroup(tr("Çizimler"));
        illustrations->addLargeAction(a("writer.image"));

        RibbonGroup *links = insert->addGroup(tr("Bağlantılar"));
        links->addLargeAction(a("writer.link"));

        RibbonGroup *text = insert->addGroup(tr("Metin"));
        text->addSmallAction(a("writer.dateTime"));
        text->addSmallAction(a("writer.hr"));
        QMenu *symbolMenu = WriterDialogs::createSymbolMenu(insert, [this](const QString &s) {
            QTextCursor c = textCursor();
            c.insertText(s);
            applyCursor(c);
        });
        text->addSmallAction(a("writer.symbol"), symbolMenu, false);

        RibbonGroup *refs = insert->addGroup(tr("Başvurular"));
        refs->addLargeAction(a("writer.toc"));
    }
    pages << insert;

    // Layout -------------------------------------------------------------------------
    auto *layout = new RibbonPage(tr("Düzen"));
    {
        RibbonGroup *setup = layout->addGroup(tr("Sayfa Yapısı"));
        auto *marginMenu = new QMenu(layout);
        const QList<QPair<QString, QMarginsF>> margins = {
            {tr("Normal (2,54 cm)"), QMarginsF(25.4, 25.4, 25.4, 25.4)},
            {tr("Dar (1,27 cm)"), QMarginsF(12.7, 12.7, 12.7, 12.7)},
            {tr("Orta (1,91 cm yanlar)"), QMarginsF(19.1, 25.4, 19.1, 25.4)},
            {tr("Geniş (5,08 cm yanlar)"), QMarginsF(50.8, 25.4, 50.8, 25.4)}};
        for (const auto &m : margins) {
            const QMarginsF value = m.second;
            marginMenu->addAction(m.first, this, [this, value] {
                PageSetup s = m_setup;
                s.margins = value;
                applyPageSetup(s);
            });
        }
        marginMenu->addSeparator();
        marginMenu->addAction(tr("Özel Kenar Boşlukları..."), this, [this] { showPageSetupDialog(); });
        setup->addLargeAction(a("writer.margins"), marginMenu, false);

        auto *orientationMenu = new QMenu(layout);
        orientationMenu->addAction(tr("Dikey"), this, [this] {
            PageSetup s = m_setup;
            s.orientation = QPageLayout::Portrait;
            applyPageSetup(s);
        });
        orientationMenu->addAction(tr("Yatay"), this, [this] {
            PageSetup s = m_setup;
            s.orientation = QPageLayout::Landscape;
            applyPageSetup(s);
        });
        setup->addLargeAction(a("writer.orientation"), orientationMenu, false);

        auto *sizeMenu = new QMenu(layout);
        for (const auto &size : WriterDialogs::pageSizes()) {
            const QPageSize::PageSizeId id = size.second;
            sizeMenu->addAction(size.first, this, [this, id] {
                PageSetup s = m_setup;
                s.pageSize = QPageSize(id);
                applyPageSetup(s);
            });
        }
        setup->addLargeAction(a("writer.pageSize"), sizeMenu, false);
        setup->setLauncher(a("writer.pageSetup"));

        RibbonGroup *hf = layout->addGroup(tr("Üst ve Alt Bilgi"));
        hf->addLargeAction(a("writer.headerFooter"));
        hf->addLargeAction(a("writer.pageNumbers"));

        RibbonGroup *spacing = layout->addGroup(tr("Paragraf Aralığı"));
        m_spaceBefore = new QDoubleSpinBox;
        m_spaceAfter = new QDoubleSpinBox;
        for (QDoubleSpinBox *s : {m_spaceBefore, m_spaceAfter}) {
            s->setRange(0, 200);
            s->setDecimals(0);
            s->setSingleStep(6);
            s->setSuffix(QStringLiteral(" nk"));
            s->setFixedWidth(80);
            s->setFocusPolicy(Qt::ClickFocus);
        }
        connect(m_spaceBefore, &QDoubleSpinBox::valueChanged, this, [this](double v) {
            QTextCursor c = textCursor();
            TextOps::setParagraphSpacing(c, v, -1);
            m_view->setTextCursor(c);
        });
        connect(m_spaceAfter, &QDoubleSpinBox::valueChanged, this, [this](double v) {
            QTextCursor c = textCursor();
            TextOps::setParagraphSpacing(c, -1, v);
            m_view->setTextCursor(c);
        });
        auto label = [](const QString &text) {
            auto *l = new QLabel(text);
            l->setFixedWidth(46);
            return l;
        };
        spacing->addRow({label(tr("Önce:")), m_spaceBefore});
        spacing->addRow({label(tr("Sonra:")), m_spaceAfter});
    }
    pages << layout;

    // Table (contextual) ----------------------------------------------------------------
    auto *table = new RibbonPage(tr("Tablo"));
    {
        RibbonGroup *rows = table->addGroup(tr("Satırlar ve Sütunlar"));
        rows->addSmallAction(a("writer.rowAbove"));
        rows->addSmallAction(a("writer.rowBelow"));
        rows->addSmallAction(a("writer.deleteRow"));
        rows->addSmallAction(a("writer.colLeft"));
        rows->addSmallAction(a("writer.colRight"));
        rows->addSmallAction(a("writer.deleteColumn"));
        RibbonGroup *merge = table->addGroup(tr("Birleştir"));
        merge->addLargeAction(a("writer.mergeCells"));
        merge->addLargeAction(a("writer.splitCell"));
        RibbonGroup *look = table->addGroup(tr("Görünüm"));
        m_cellColor = new ColorButton(QStringLiteral("fill-color"));
        m_cellColor->setColor(QColor(0xDD, 0xEB, 0xF7));
        m_cellColor->setNoneText(tr("Renk yok"));
        m_cellColor->setToolTip(tr("Hücre Gölgelendirme"));
        connect(m_cellColor, &ColorButton::colorPicked, this, &WriterEditor::setCellColor);
        look->addRow({m_cellColor, RibbonGroup::iconButton(a("writer.tableBorders"))});
        look->addSmallAction(a("writer.deleteTable"));
    }
    table->setTabVisible(false);
    m_tablePage = table;
    pages << table;

    // Review -------------------------------------------------------------------------
    auto *review = new RibbonPage(tr("Gözden Geçir"));
    {
        RibbonGroup *proof = review->addGroup(tr("Yazım"));
        proof->addLargeAction(a("writer.wordCount"));
        RibbonGroup *share = review->addGroup(tr("Paylaş"));
        share->addLargeAction(a("writer.sendMail"));
    }
    pages << review;

    // View ---------------------------------------------------------------------------
    auto *view = new RibbonPage(tr("Görünüm"));
    {
        RibbonGroup *show = view->addGroup(tr("Göster"));
        show->addSmallAction(a("writer.navPane"));
        show->addSmallAction(a("writer.showMarks"));
        show->addSmallAction(a("writer.darkPage"));
        RibbonGroup *zoom = view->addGroup(tr("Yakınlaştır"));
        zoom->addLargeAction(a("writer.zoom100"));
        zoom->addLargeAction(a("writer.onePage"));
        zoom->addLargeAction(a("writer.fitWidth"));
    }
    pages << view;

    updateFormatState();
    return pages;
}

// ---------------------------------------------------------------------------------
// Formatting

void WriterEditor::mergeFormat(const QTextCharFormat &format)
{
    QTextCursor c = textCursor();
    TextOps::mergeCharFormat(c, format);
    applyCursor(c);
}

void WriterEditor::clearProperty(int property)
{
    QTextCursor c = textCursor();
    TextOps::clearCharProperty(c, property);
    applyCursor(c);
}

void WriterEditor::growFont(int direction)
{
    const QTextCursor c = textCursor();
    const qreal current = c.charFormat().fontPointSize() > 0 ? c.charFormat().fontPointSize()
                                                            : m_doc->defaultFont().pointSizeF();
    qreal next = current;
    if (direction > 0) {
        next = current + 1;
        for (qreal s : kFontSizes) {
            if (s > current) {
                next = s;
                break;
            }
        }
    } else {
        next = qMax<qreal>(1, current - 1);
        for (int i = int(kFontSizes.size()) - 1; i >= 0; --i) {
            if (kFontSizes.at(i) < current) {
                next = kFontSizes.at(i);
                break;
            }
        }
    }
    QTextCharFormat f;
    f.setFontPointSize(next);
    mergeFormat(f);
}

void WriterEditor::changeCaseCycle()
{
    QTextCursor c = textCursor();
    if (!c.hasSelection())
        c.select(QTextCursor::WordUnderCursor);
    const QString text = c.selectedText();
    const QLocale tr(QLocale::Turkish, QLocale::Turkey);
    TextOps::CaseMode mode = TextOps::CaseMode::Upper;
    if (text == tr.toUpper(text))
        mode = TextOps::CaseMode::Capitalize;
    else if (text != tr.toLower(text))
        mode = TextOps::CaseMode::Lower;
    TextOps::changeCase(c, mode);
    applyCursor(c);
}

void WriterEditor::updateFormatState()
{
    if (m_actions.isEmpty())
        return;
    const QTextCursor c = textCursor();
    const QTextCharFormat f = c.charFormat();
    const QFont font = f.font().resolve(m_doc->defaultFont());
    auto set = [this](const char *id, bool on) {
        if (QAction *a = m_actions.value(QString::fromLatin1(id))) {
            const QSignalBlocker blocker(a);
            a->setChecked(on);
        }
    };
    set("writer.bold", font.bold());
    set("writer.italic", font.italic());
    set("writer.underline", font.underline());
    set("writer.strike", font.strikeOut());
    set("writer.subscript", f.verticalAlignment() == QTextCharFormat::AlignSubScript);
    set("writer.superscript", f.verticalAlignment() == QTextCharFormat::AlignSuperScript);

    const Qt::Alignment align = c.blockFormat().alignment();
    const bool center = align.testFlag(Qt::AlignHCenter);
    const bool right = align.testFlag(Qt::AlignRight) && !center;
    const bool justify = align.testFlag(Qt::AlignJustify);
    set("writer.alignCenter", center);
    set("writer.alignRight", right);
    set("writer.alignJustify", justify);
    set("writer.alignLeft", !center && !right && !justify);

    const QTextList *list = c.currentList();
    const bool checklist = list && c.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker;
    const QTextListFormat::Style style = list ? list->format().style() : QTextListFormat::ListStyleUndefined;
    const bool bullet = style == QTextListFormat::ListDisc || style == QTextListFormat::ListCircle
                        || style == QTextListFormat::ListSquare;
    set("writer.bullets", list && bullet && !checklist);
    set("writer.numbering", list && !bullet && !checklist);
    set("writer.checklist", checklist);
    set("writer.pageNumbers", m_setup.footer.contains(QLatin1String("{page}")));

    if (m_fontCombo) {
        const QSignalBlocker b1(m_fontCombo);
        const QSignalBlocker b2(m_sizeCombo);
        m_fontCombo->setCurrentFont(font);
        m_sizeCombo->setEditText(QLocale().toString(font.pointSizeF()));
    }
    if (m_spaceBefore) {
        const QSignalBlocker b1(m_spaceBefore);
        const QSignalBlocker b2(m_spaceAfter);
        m_spaceBefore->setValue(c.blockFormat().topMargin() * 72.0 / 96.0);
        m_spaceAfter->setValue(c.blockFormat().bottomMargin() * 72.0 / 96.0);
    }
    const QString styleId = TextOps::styleIdOf(c.block());
    for (QToolButton *b : std::as_const(m_styleButtons))
        b->setChecked(b->property("styleId").toString() == styleId);

    const bool inTable = c.currentTable() != nullptr;
    if (m_tablePage)
        m_tablePage->setTabVisible(inTable);
    for (const char *id : {"writer.rowAbove", "writer.rowBelow", "writer.colLeft", "writer.colRight", "writer.deleteRow",
                           "writer.deleteColumn", "writer.deleteTable", "writer.mergeCells", "writer.splitCell",
                           "writer.tableBorders"}) {
        m_actions.value(QString::fromLatin1(id))->setEnabled(inTable);
    }
    if (inTable)
        set("writer.tableBorders", c.currentTable()->format().border() > 0);
    m_actions.value(QStringLiteral("writer.removeLink"))->setEnabled(!f.anchorHref().isEmpty());
}

// ---------------------------------------------------------------------------------
// Status bar

QWidget *WriterEditor::statusWidget()
{
    if (!m_status) {
        auto *w = new QWidget;
        auto *layout = new QHBoxLayout(w);
        layout->setContentsMargins(4, 0, 0, 0);
        layout->setSpacing(4);
        m_pageButton = new QToolButton(w);
        m_pageButton->setAutoRaise(true);
        m_pageButton->setToolTip(tr("Sayfaya git"));
        connect(m_pageButton, &QToolButton::clicked, this, [this] {
            bool ok = false;
            const int page = QInputDialog::getInt(this, tr("Git"), tr("Sayfa numarası:"), m_view->currentPage(), 1,
                                                  m_view->pageCount(), 1, &ok);
            if (!ok)
                return;
            const QSizeF size = m_setup.sizePx();
            const qreal y = (page - 1) * (size.height() + 18) + m_setup.marginsPx().top() + 2;
            const int pos = m_doc->documentLayout()->hitTest(QPointF(size.width() / 2, y), Qt::FuzzyHit);
            if (pos >= 0) {
                QTextCursor c(m_doc);
                c.setPosition(pos);
                c.movePosition(QTextCursor::StartOfBlock);
                m_view->setTextCursor(c);
                m_view->scrollToPosition(c.position());
                m_view->focusText();
            }
        });
        m_wordsButton = new QToolButton(w);
        m_wordsButton->setAutoRaise(true);
        m_wordsButton->setToolTip(tr("Sözcük sayısı ayrıntıları"));
        connect(m_wordsButton, &QToolButton::clicked, this, &WriterEditor::showWordCount);
        auto *language = new QLabel(tr("Türkçe (Türkiye)"), w);
        layout->addWidget(m_pageButton);
        layout->addWidget(m_wordsButton);
        layout->addWidget(language);
        layout->addStretch(1);
        m_status = w;
        updateStatus();
    }
    return m_status;
}

void WriterEditor::scheduleStatus()
{
    m_statusTimer->start();
}

void WriterEditor::updateStatus()
{
    if (!m_status)
        return;
    m_pageButton->setText(tr("Sayfa %1 / %2").arg(m_view->currentPage()).arg(m_view->pageCount()));
    const QTextCursor c = textCursor();
    const int total = TextOps::wordCount(m_doc->toPlainText());
    if (c.hasSelection())
        m_wordsButton->setText(tr("%1 / %2 sözcük").arg(TextOps::wordCount(c.selectedText())).arg(total));
    else
        m_wordsButton->setText(tr("%1 sözcük").arg(QLocale().toString(total)));
}

void WriterEditor::showWordCount()
{
    const QString text = m_doc->toPlainText();
    int noSpaces = 0;
    for (const QChar ch : text) {
        if (!ch.isSpace())
            ++noSpaces;
    }
    int paragraphs = 0;
    for (QTextBlock b = m_doc->begin(); b.isValid(); b = b.next()) {
        if (!b.text().trimmed().isEmpty())
            ++paragraphs;
    }
    const QLocale locale;
    QMessageBox box(this);
    box.setWindowTitle(tr("Sözcük Sayısı"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr("<table cellspacing=6>"
                   "<tr><td>Sayfa</td><td align=right><b>%1</b></td></tr>"
                   "<tr><td>Sözcük</td><td align=right><b>%2</b></td></tr>"
                   "<tr><td>Karakter (boşluksuz)</td><td align=right><b>%3</b></td></tr>"
                   "<tr><td>Karakter (boşluklu)</td><td align=right><b>%4</b></td></tr>"
                   "<tr><td>Paragraf</td><td align=right><b>%5</b></td></tr></table>")
                    .arg(locale.toString(m_view->pageCount()), locale.toString(TextOps::wordCount(text)),
                         locale.toString(noSpaces), locale.toString(text.size()), locale.toString(paragraphs)));
    box.exec();
}

// ---------------------------------------------------------------------------------
// Find & replace

void WriterEditor::showFind()
{
    m_find->setReplaceEnabled(true);
    m_find->showFind(textCursor().selectedText());
}

void WriterEditor::showReplace()
{
    m_find->setReplaceEnabled(true);
    m_find->showReplace(textCursor().selectedText());
}

void WriterEditor::runSearch()
{
    if (!m_find->isVisible())
        return;
    m_matches = TextOps::findAll(m_doc, m_find->text(), m_find->caseSensitive(), m_find->wholeWords(),
                                 m_find->regularExpression());
    m_currentMatch = -1;
    const int pos = textCursor().selectionStart();
    for (int i = 0; i < m_matches.size(); ++i) {
        if (m_matches.at(i).selectionStart() >= pos) {
            m_currentMatch = i;
            break;
        }
    }
    if (m_currentMatch < 0 && !m_matches.isEmpty())
        m_currentMatch = 0;
    updateHighlights();
}

void WriterEditor::updateHighlights()
{
    QList<QRectF> all;
    QList<QRectF> current;
    for (int i = 0; i < m_matches.size() && i < 2000; ++i) {
        const QList<QRectF> rects = TextOps::rangeRects(m_doc, m_matches.at(i).selectionStart(),
                                                        m_matches.at(i).selectionEnd());
        (i == m_currentMatch ? current : all) << rects;
    }
    m_view->textItem()->setHighlights(all, current);
    m_find->setMatchInfo(m_currentMatch >= 0 ? m_currentMatch + 1 : 0, int(m_matches.size()));
}

void WriterEditor::findStep(bool forward)
{
    if (m_matches.isEmpty())
        runSearch();
    if (m_matches.isEmpty())
        return;
    const QTextCursor c = textCursor();
    const bool onCurrent = m_currentMatch >= 0 && c.selectionStart() == m_matches.at(m_currentMatch).selectionStart()
                           && c.selectionEnd() == m_matches.at(m_currentMatch).selectionEnd();
    if (onCurrent || m_currentMatch < 0) {
        const int n = int(m_matches.size());
        m_currentMatch = m_currentMatch < 0 ? 0 : (m_currentMatch + (forward ? 1 : n - 1)) % n;
    }
    m_view->setTextCursor(m_matches.at(m_currentMatch));
    m_view->ensureCursorVisible();
    updateHighlights();
    updateFormatState();
}

void WriterEditor::replaceCurrent()
{
    if (m_matches.isEmpty() || m_currentMatch < 0) {
        findStep(true);
        return;
    }
    QTextCursor match = m_matches.at(m_currentMatch);
    const QTextCursor c = textCursor();
    if (c.selectionStart() != match.selectionStart() || c.selectionEnd() != match.selectionEnd()) {
        findStep(true);
        return;
    }
    QString replacement = m_find->replacementText();
    if (m_find->regularExpression()) {
        const QRegularExpression re(m_find->text(), m_find->caseSensitive() ? QRegularExpression::NoPatternOption
                                                                           : QRegularExpression::CaseInsensitiveOption);
        replacement = match.selectedText().replace(re, replacement);
    }
    match.insertText(replacement);
    runSearch();
    if (!m_matches.isEmpty())
        findStep(true);
}

void WriterEditor::replaceEverything()
{
    runSearch();
    if (m_matches.isEmpty())
        return;
    const QRegularExpression re(m_find->text(), m_find->caseSensitive() ? QRegularExpression::NoPatternOption
                                                                       : QRegularExpression::CaseInsensitiveOption);
    QTextCursor edit(m_doc);
    edit.beginEditBlock();
    for (int i = int(m_matches.size()) - 1; i >= 0; --i) {
        QTextCursor match = m_matches.at(i);
        QString replacement = m_find->replacementText();
        if (m_find->regularExpression())
            replacement = match.selectedText().replace(re, replacement);
        match.insertText(replacement);
    }
    edit.endEditBlock();
    const int count = int(m_matches.size());
    runSearch();
    showMessage(tr("%1 değişiklik yapıldı.").arg(count));
}

// ---------------------------------------------------------------------------------
// Clipboard & insertion

void WriterEditor::copySelection(bool cut)
{
    QTextCursor c = textCursor();
    if (!c.hasSelection())
        return;
    const QTextDocumentFragment fragment = c.selection();
    auto *mime = new QMimeData;
    mime->setHtml(fragment.toHtml());
    mime->setText(fragment.toPlainText());
    QGuiApplication::clipboard()->setMimeData(mime);
    if (cut) {
        c.removeSelectedText();
        applyCursor(c);
    }
}

void WriterEditor::paste(bool plainText)
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime)
        return;
    QTextCursor c = textCursor();
    c.beginEditBlock();
    if (!plainText && mime->hasImage() && !mime->hasHtml()) {
        TextOps::insertImage(c, qvariant_cast<QImage>(mime->imageData()), textWidth());
    } else if (!plainText && mime->hasUrls() && !mime->hasText()) {
        c.endEditBlock();
        insertImageFiles(mime->urls(), c.position());
        return;
    } else if (!plainText && mime->hasHtml()) {
        c.insertHtml(mime->html());
        TextOps::embedImageResources(m_doc);
    } else if (mime->hasText()) {
        c.insertText(mime->text());
    }
    c.endEditBlock();
    applyCursor(c);
}

void WriterEditor::insertImagesFromDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Resim Ekle"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("Resimler (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.svg *.ico);;Tüm dosyalar (*)"));
    QList<QUrl> urls;
    for (const QString &f : files)
        urls << QUrl::fromLocalFile(f);
    if (!urls.isEmpty())
        insertImageFiles(urls, textCursor().position());
}

void WriterEditor::insertImageFiles(const QList<QUrl> &urls, int position)
{
    QTextCursor c = textCursor();
    if (position >= 0)
        c.setPosition(qBound(0, position, m_doc->characterCount() - 1));
    bool inserted = false;
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty())
            continue;
        if (isImageFile(path)) {
            QImage image(path);
            if (!image.isNull()) {
                TextOps::insertImage(c, image, textWidth());
                inserted = true;
            }
        } else if (ModuleRegistry::instance().moduleForFile(path)) {
            emit openFileRequested(path);
        }
    }
    if (inserted)
        applyCursor(c);
}

void WriterEditor::insertLink()
{
    QTextCursor c = textCursor();
    const QString existing = c.charFormat().anchorHref();
    LinkDialog dialog(c.selectedText(), existing, this);
    if (dialog.exec() != QDialog::Accepted || dialog.url().isEmpty())
        return;
    TextOps::insertHyperlink(c, dialog.text(), dialog.url());
    applyCursor(c);
}

void WriterEditor::openLink(const QString &href)
{
    if (href.startsWith(QLatin1Char('#'))) {
        const QString anchor = href.mid(1);
        for (QTextBlock block = m_doc->begin(); block.isValid(); block = block.next()) {
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                if (it.fragment().charFormat().anchorNames().contains(anchor)) {
                    QTextCursor c(m_doc);
                    c.setPosition(it.fragment().position());
                    m_view->setTextCursor(c);
                    m_view->scrollToPosition(c.position());
                    return;
                }
            }
        }
        return;
    }
    QDesktopServices::openUrl(QUrl(href));
}

void WriterEditor::editImage(int position)
{
    QTextCursor c(m_doc);
    c.setPosition(position);
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    QTextImageFormat f = c.charFormat().toImageFormat();
    const QVariant res = m_doc->resource(QTextDocument::ImageResource, QUrl(f.name()));
    QImage image = res.value<QImage>();
    if (image.isNull() && res.canConvert<QPixmap>())
        image = res.value<QPixmap>().toImage();
    const QSize original = image.isNull() ? QSize(int(f.width()), int(f.height())) : image.size();
    const QSizeF current(f.width() > 0 ? f.width() : original.width(), f.height() > 0 ? f.height() : original.height());
    ImageSizeDialog dialog(current, original, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    f.setWidth(dialog.imageSize().width());
    f.setHeight(dialog.imageSize().height());
    c.setCharFormat(f);
}

void WriterEditor::insertDateTime()
{
    const QLocale locale(QLocale::Turkish, QLocale::Turkey);
    const QDateTime now = QDateTime::currentDateTime();
    const QStringList options = {locale.toString(now.date(), QStringLiteral("d MMMM yyyy")),
                                 locale.toString(now.date(), QStringLiteral("d MMMM yyyy dddd")),
                                 now.date().toString(QStringLiteral("dd.MM.yyyy")),
                                 locale.toString(now, QStringLiteral("dd.MM.yyyy HH:mm")),
                                 now.time().toString(QStringLiteral("HH:mm"))};
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, tr("Tarih ve Saat"), tr("Biçim:"), options, 0, false, &ok);
    if (!ok)
        return;
    QTextCursor c = textCursor();
    c.insertText(choice);
    applyCursor(c);
}

void WriterEditor::insertTableOfContents()
{
    struct Heading
    {
        int level;
        QString text;
        int position;
    };
    QList<Heading> headings;
    for (QTextBlock block = m_doc->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        const QString text = block.text().trimmed();
        if (level >= 1 && level <= 3 && !text.isEmpty())
            headings << Heading{level, text, block.position()};
    }
    if (headings.isEmpty()) {
        QMessageBox::information(this, tr("İçindekiler"),
                                 tr("Belgede başlık bulunamadı. Önce paragraflara \"Başlık 1-3\" stillerini uygulayın."));
        return;
    }
    QTextCursor edit(m_doc);
    edit.beginEditBlock();
    // Mark every heading with a bookmark that the table of contents links to.
    for (int i = 0; i < headings.size(); ++i) {
        QTextCursor hc(m_doc);
        hc.setPosition(headings.at(i).position);
        hc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextCharFormat mark;
        mark.setAnchor(true);
        mark.setAnchorNames({QStringLiteral("_Toc%1").arg(i + 1)});
        hc.mergeCharFormat(mark);
    }
    const qreal stride = m_setup.sizePx().height() + 18;
    QTextCursor c = textCursor();
    if (c.block().length() > 1)
        c.insertBlock();
    TextOps::applyParagraphStyle(c, QStringLiteral("Heading1"));
    QTextBlockFormat titleFormat = c.blockFormat();
    titleFormat.setHeadingLevel(0);
    titleFormat.setProperty(TextOps::StyleIdProperty, QStringLiteral("Heading1"));
    c.setBlockFormat(titleFormat);
    c.insertText(tr("İçindekiler"));
    for (int i = 0; i < headings.size(); ++i) {
        const Heading &h = headings.at(i);
        QTextBlockFormat bf;
        bf.setLeftMargin((h.level - 1) * 24);
        bf.setBottomMargin(TextOps::pointsToPixels(4));
        bf.setTabPositions({QTextOption::Tab(textWidth() - 4, QTextOption::RightTab)});
        c.insertBlock(bf, QTextCharFormat());
        const QRectF rect = m_doc->documentLayout()->blockBoundingRect(m_doc->findBlock(h.position));
        const int page = int(rect.top() / stride) + 1;
        QTextCharFormat link;
        link.setAnchor(true);
        link.setAnchorHref(QStringLiteral("#_Toc%1").arg(i + 1));
        if (h.level == 1)
            link.setFontWeight(QFont::DemiBold);
        c.insertText(h.text + QLatin1Char('\t') + QString::number(page), link);
    }
    c.insertBlock(QTextBlockFormat(), QTextCharFormat());
    TextOps::applyParagraphStyle(c, QStringLiteral("Normal"));
    edit.endEditBlock();
    applyCursor(c);
}

void WriterEditor::showContextMenu(const QPoint &screenPos)
{
    auto a = [this](const char *id) { return m_actions.value(QString::fromLatin1(id)); };
    QMenu menu(this);
    const QTextCursor c = textCursor();
    menu.addAction(a("writer.cut"));
    menu.addAction(a("writer.copy"));
    menu.addAction(a("writer.paste"));
    menu.addAction(a("writer.pastePlain"));
    a("writer.cut")->setEnabled(c.hasSelection());
    a("writer.copy")->setEnabled(c.hasSelection());
    menu.addSeparator();

    const QString href = c.charFormat().anchorHref();
    if (!href.isEmpty()) {
        menu.addAction(Icons::icon(QStringLiteral("open-external")), tr("Bağlantıyı Aç"), this,
                       [this, href] { openLink(href); });
        menu.addAction(Icons::icon(QStringLiteral("link")), tr("Bağlantıyı Düzenle..."), this, &WriterEditor::insertLink);
        menu.addAction(a("writer.removeLink"));
        menu.addSeparator();
    }
    QTextCursor imageProbe = c;
    imageProbe.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    if (imageProbe.charFormat().isImageFormat()) {
        const int pos = c.position();
        menu.addAction(Icons::icon(QStringLiteral("image")), tr("Resim Boyutu..."), this,
                       [this, pos] { editImage(pos); });
        menu.addSeparator();
    }
    if (c.currentTable()) {
        QMenu *tableMenu = menu.addMenu(Icons::icon(QStringLiteral("table")), tr("Tablo"));
        for (const char *id : {"writer.rowAbove", "writer.rowBelow", "writer.colLeft", "writer.colRight"})
            tableMenu->addAction(a(id));
        tableMenu->addSeparator();
        for (const char *id : {"writer.deleteRow", "writer.deleteColumn", "writer.mergeCells", "writer.splitCell",
                               "writer.deleteTable"})
            tableMenu->addAction(a(id));
        menu.addSeparator();
    }
    QMenu *styles = menu.addMenu(Icons::icon(QStringLiteral("styles")), tr("Stil"));
    for (const TextOps::ParagraphStyle &style : TextOps::paragraphStyles()) {
        const QString id = style.id;
        styles->addAction(style.name, this, [this, id] {
            QTextCursor cc = textCursor();
            TextOps::applyParagraphStyle(cc, id);
            applyCursor(cc);
        });
    }
    menu.addAction(a("writer.link"));
    menu.addAction(a("writer.clearFormat"));
    menu.exec(screenPos);
    a("writer.cut")->setEnabled(true);
    a("writer.copy")->setEnabled(true);
}

void WriterEditor::tableCommand(const QString &command)
{
    QTextCursor c = textCursor();
    QTextTable *table = c.currentTable();
    if (!table)
        return;
    const QTextTableCell cell = table->cellAt(c);
    int firstRow = cell.row();
    int numRows = 1;
    int firstCol = cell.column();
    int numCols = 1;
    if (c.hasSelection()) {
        int fr = -1, nr = 0, fc = -1, nc = 0;
        c.selectedTableCells(&fr, &nr, &fc, &nc);
        if (fr >= 0) {
            firstRow = fr;
            numRows = nr;
            firstCol = fc;
            numCols = nc;
        }
    }
    auto normalizeWidths = [table] {
        QTextTableFormat tf = table->format();
        QList<QTextLength> widths;
        for (int i = 0; i < table->columns(); ++i)
            widths << QTextLength(QTextLength::PercentageLength, 100.0 / table->columns());
        tf.setColumnWidthConstraints(widths);
        table->setFormat(tf);
    };

    c.beginEditBlock();
    if (command == QLatin1String("writer.rowAbove")) {
        table->insertRows(cell.row(), 1);
    } else if (command == QLatin1String("writer.rowBelow")) {
        table->insertRows(cell.row() + cell.rowSpan(), 1);
    } else if (command == QLatin1String("writer.colLeft")) {
        table->insertColumns(cell.column(), 1);
        normalizeWidths();
    } else if (command == QLatin1String("writer.colRight")) {
        table->insertColumns(cell.column() + cell.columnSpan(), 1);
        normalizeWidths();
    } else if (command == QLatin1String("writer.deleteRow")) {
        if (numRows >= table->rows()) {
            c.endEditBlock();
            tableCommand(QStringLiteral("writer.deleteTable"));
            return;
        }
        table->removeRows(firstRow, numRows);
    } else if (command == QLatin1String("writer.deleteColumn")) {
        if (numCols >= table->columns()) {
            c.endEditBlock();
            tableCommand(QStringLiteral("writer.deleteTable"));
            return;
        }
        table->removeColumns(firstCol, numCols);
        normalizeWidths();
    } else if (command == QLatin1String("writer.deleteTable")) {
        QTextCursor range(m_doc);
        range.setPosition(table->firstPosition() - 1);
        range.setPosition(table->lastPosition() + 1, QTextCursor::KeepAnchor);
        range.removeSelectedText();
        c = range;
    } else if (command == QLatin1String("writer.mergeCells")) {
        if (c.hasSelection())
            table->mergeCells(c);
    } else if (command == QLatin1String("writer.splitCell")) {
        if (cell.rowSpan() > 1 || cell.columnSpan() > 1)
            table->splitCell(cell.row(), cell.column(), 1, 1);
    } else if (command == QLatin1String("writer.tableBorders")) {
        QTextTableFormat tf = table->format();
        const bool on = tf.border() <= 0;
        tf.setBorder(on ? 1 : 0);
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        tf.setBorderBrush(QColor(0x8C, 0x8C, 0x8C));
        table->setFormat(tf);
    }
    c.endEditBlock();
    applyCursor(c);
}

void WriterEditor::setCellColor(const QColor &color)
{
    QTextCursor c = textCursor();
    QTextTable *table = c.currentTable();
    if (!table)
        return;
    int firstRow = table->cellAt(c).row();
    int numRows = 1;
    int firstCol = table->cellAt(c).column();
    int numCols = 1;
    if (c.hasSelection()) {
        int fr = -1, nr = 0, fc = -1, nc = 0;
        c.selectedTableCells(&fr, &nr, &fc, &nc);
        if (fr >= 0) {
            firstRow = fr;
            numRows = nr;
            firstCol = fc;
            numCols = nc;
        }
    }
    c.beginEditBlock();
    for (int r = firstRow; r < firstRow + numRows; ++r) {
        for (int col = firstCol; col < firstCol + numCols; ++col) {
            QTextTableCell cell = table->cellAt(r, col);
            QTextTableCellFormat f = cell.format().toTableCellFormat();
            if (color.isValid())
                f.setBackground(color);
            else
                f.clearBackground();
            cell.setFormat(f);
        }
    }
    c.endEditBlock();
}

bool WriterEditor::handleKey(QKeyEvent *event)
{
    QTextCursor c = textCursor();
    const bool tab = event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab;
    if (tab && !(event->modifiers() & Qt::ControlModifier)) {
        const bool back = event->key() == Qt::Key_Backtab || (event->modifiers() & Qt::ShiftModifier);
        if (QTextTable *table = c.currentTable()) {
            const QTextTableCell cell = table->cellAt(c);
            int row = cell.row();
            int col = cell.column();
            if (!back) {
                col += cell.columnSpan();
                if (col >= table->columns()) {
                    col = 0;
                    ++row;
                    if (row >= table->rows())
                        table->appendRows(1);
                }
            } else {
                --col;
                if (col < 0) {
                    col = table->columns() - 1;
                    --row;
                    if (row < 0)
                        return true;
                }
            }
            const QTextTableCell target = table->cellAt(row, col);
            QTextCursor nc = target.firstCursorPosition();
            nc.setPosition(target.lastCursorPosition().position(), QTextCursor::KeepAnchor);
            applyCursor(nc);
            return true;
        }
        if (c.currentList() && (c.positionInBlock() == 0 || c.hasSelection())) {
            TextOps::changeIndent(c, back ? -1 : 1);
            applyCursor(c);
            return true;
        }
    }
    const bool enter = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier;
    if (enter && c.currentList() && c.block().text().isEmpty()) {
        // Enter on an empty list item ends the list, as in word processors.
        TextOps::changeIndent(c, -1);
        applyCursor(c);
        return true;
    }
    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !c.hasSelection()
        && c.currentList() && c.positionInBlock() == 0) {
        TextOps::changeIndent(c, -(c.currentList()->format().indent()));
        applyCursor(c);
        return true;
    }
    return false;
}

void WriterEditor::rebuildOutline()
{
    m_outline->clear();
    QList<QTreeWidgetItem *> stack;
    for (QTextBlock block = m_doc->begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        const QString text = block.text().trimmed();
        if (level < 1 || level > 6 || text.isEmpty())
            continue;
        while (stack.size() >= level)
            stack.removeLast();
        auto *item = new QTreeWidgetItem({text});
        item->setData(0, Qt::UserRole, block.position());
        item->setToolTip(0, text);
        if (stack.isEmpty())
            m_outline->addTopLevelItem(item);
        else
            stack.last()->addChild(item);
        while (stack.size() < level - 1)
            stack << item;
        stack << item;
    }
    m_outline->expandAll();
    if (m_outline->topLevelItemCount() == 0) {
        auto *hint = new QTreeWidgetItem({tr("Başlık yok. Başlık stillerini kullanın.")});
        hint->setFlags(Qt::NoItemFlags);
        m_outline->addTopLevelItem(hint);
    }
}

// ---------------------------------------------------------------------------------
// Page setup

void WriterEditor::applyPageSetup(const PageSetup &setup)
{
    if (setup == m_setup)
        return;
    m_setup = setup;
    m_view->setPageSetup(m_setup);
    m_doc->setModified(true);
    updateFormatState();
    scheduleStatus();
}

void WriterEditor::showPageSetupDialog()
{
    PageSetupDialog dialog(m_setup, this);
    if (dialog.exec() == QDialog::Accepted)
        applyPageSetup(dialog.pageSetup());
}

// ---------------------------------------------------------------------------------
// Document life cycle

void WriterEditor::newDocument()
{
    m_view->focusText();
}

bool WriterEditor::applyTemplate(const QString &templateId)
{
    if (templateId == QLatin1String("writer.blank"))
        return true;
    if (!WriterTemplates::build(templateId, m_doc, &m_setup))
        return false;
    m_view->setPageSetup(m_setup);
    QTextCursor c(m_doc);
    m_view->setTextCursor(c);
    updateFormatState();
    scheduleStatus();
    return true;
}

bool WriterEditor::readFile(const QString &path, QString *errorMessage)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    m_doc->baseDirectory = QFileInfo(path).absolutePath();
    PageSetup setup;
    if (ext == QLatin1String("docx")) {
        DocxMetadata meta;
        if (!Docx::read(path, m_doc, &setup, &meta, errorMessage))
            return false;
        m_title = meta.title;
        m_author = meta.author;
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage)
                *errorMessage = tr("Dosya açılamadı: %1").arg(file.errorString());
            return false;
        }
        const QString text = TextCodec::decode(file.readAll());
        m_doc->setUndoRedoEnabled(false);
        m_doc->clear();
        m_doc->setDefaultFont(TextOps::defaultFont());
        m_doc->setIndentWidth(48);
        if (ext == QLatin1String("html") || ext == QLatin1String("htm"))
            m_doc->setHtml(text);
        else if (ext == QLatin1String("md") || ext == QLatin1String("markdown"))
            m_doc->setMarkdown(text);
        else
            m_doc->setPlainText(text);
        TextOps::embedImageResources(m_doc);
        m_doc->setUndoRedoEnabled(true);
    }
    m_fileFormat = ext;
    m_setup = setup;
    m_view->setPageSetup(m_setup);
    m_view->setTextCursor(QTextCursor(m_doc));
    m_doc->clearUndoRedoStacks();
    m_doc->setModified(false);
    updateFormatState();
    scheduleStatus();
    if (m_outlinePanel->isVisible())
        rebuildOutline();
    return true;
}

QString WriterEditor::documentTitleGuess() const
{
    if (!m_title.isEmpty())
        return m_title;
    for (QTextBlock block = m_doc->begin(); block.isValid(); block = block.next()) {
        const QString text = block.text().trimmed();
        if (!text.isEmpty())
            return text.left(80);
    }
    return documentTitle();
}

bool WriterEditor::writeFormat(const QString &path, const QString &format, QString *errorMessage)
{
    if (format == QLatin1String("docx")) {
        DocxMetadata meta;
        meta.title = documentTitleGuess();
        meta.author = m_author.isEmpty() ? AppSettings::instance().userName() : m_author;
        return Docx::write(path, m_doc, m_setup, meta, errorMessage);
    }
    if (format == QLatin1String("html") || format == QLatin1String("htm")) {
        QString html = m_doc->toHtml();
        html = saveImagesBesides(m_doc, path, html, false);
        return writeText(path, html, errorMessage);
    }
    if (format == QLatin1String("md") || format == QLatin1String("markdown")) {
        QString md = m_doc->toMarkdown();
        md = saveImagesBesides(m_doc, path, md, true);
        return writeText(path, md, errorMessage);
    }
    if (format == QLatin1String("txt") || format == QLatin1String("text") || format == QLatin1String("log"))
        return writeText(path, m_doc->toPlainText(), errorMessage);
    if (format == QLatin1String("odt")) {
        QTextDocumentWriter writer(path, "odf");
        if (!writer.write(m_doc)) {
            if (errorMessage)
                *errorMessage = tr("ODT dosyası yazılamadı.");
            return false;
        }
        return true;
    }
    if (format == QLatin1String("pdf")) {
        return DocumentPrinter::exportPdf(m_doc, m_setup, path, documentTitleGuess(),
                                          AppSettings::instance().userName(), errorMessage);
    }
    if (errorMessage)
        *errorMessage = tr("Desteklenmeyen dosya biçimi: %1").arg(format);
    return false;
}

bool WriterEditor::writeFile(const QString &path, QString *errorMessage)
{
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty())
        ext = QStringLiteral("docx");
    return writeFormat(path, ext, errorMessage);
}

void WriterEditor::fileSaved(const QString &path)
{
    m_fileFormat = QFileInfo(path).suffix().toLower();
    m_doc->setModified(false);
}

QString WriterEditor::saveFilter() const
{
    return tr("Word Belgesi (*.docx);;Web Sayfası (*.html *.htm);;Markdown (*.md);;Düz Metin (*.txt)");
}

bool WriterEditor::canSaveInPlace() const
{
    static const QStringList writable = {QStringLiteral("docx"), QStringLiteral("html"), QStringLiteral("htm"),
                                         QStringLiteral("md"),   QStringLiteral("markdown"), QStringLiteral("txt"),
                                         QStringLiteral("text"), QStringLiteral("log")};
    return !isUntitled() && writable.contains(QFileInfo(filePath()).suffix().toLower());
}

QList<ExportFormat> WriterEditor::exportFormats() const
{
    return {{QStringLiteral("pdf"), tr("PDF belgesi"), tr("PDF belgesi (*.pdf)"), QStringLiteral("pdf")},
            {QStringLiteral("docx"), tr("Word belgesi"), tr("Word Belgesi (*.docx)"), QStringLiteral("docx")},
            {QStringLiteral("odt"), tr("OpenDocument metni"), tr("OpenDocument Metni (*.odt)"), QStringLiteral("odt")},
            {QStringLiteral("html"), tr("Web sayfası"), tr("Web Sayfası (*.html)"), QStringLiteral("html")},
            {QStringLiteral("md"), tr("Markdown"), tr("Markdown (*.md)"), QStringLiteral("md")},
            {QStringLiteral("txt"), tr("Düz metin"), tr("Düz Metin (*.txt)"), QStringLiteral("txt")}};
}

bool WriterEditor::exportTo(const QString &formatId, const QString &path, QString *errorMessage)
{
    const bool modified = m_doc->isModified();
    const bool ok = writeFormat(path, formatId, errorMessage);
    m_doc->setModified(modified);
    return ok;
}

void WriterEditor::print(QPrinter *printer)
{
    printer->setFullPage(true);
    printer->setPageLayout(m_setup.pageLayout());
    DocumentPrinter::paint(m_doc, m_setup, printer);
}

void WriterEditor::sendByMail()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/OmniOffice");
    QDir().mkpath(dir);
    QString base = isUntitled() ? documentTitle() : QFileInfo(filePath()).completeBaseName();
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString path = dir + QLatin1Char('/') + base + QStringLiteral(".docx");
    QString error;
    if (!exportTo(QStringLiteral("docx"), path, &error)) {
        QMessageBox::warning(this, tr("E-postayla Gönder"), error);
        return;
    }
    emit sendByMailRequested({path}, base);
}

// ---------------------------------------------------------------------------------
// Shell commands

bool WriterEditor::canUndo() const { return m_doc->isUndoAvailable(); }
bool WriterEditor::canRedo() const { return m_doc->isRedoAvailable(); }

void WriterEditor::undo()
{
    QTextCursor c = textCursor();
    m_doc->undo(&c);
    applyCursor(c);
}

void WriterEditor::redo()
{
    QTextCursor c = textCursor();
    m_doc->redo(&c);
    applyCursor(c);
}

int WriterEditor::zoomPercent() const { return m_view->zoomPercent(); }
void WriterEditor::setZoomPercent(int percent) { m_view->setZoomPercent(percent); }

void WriterEditor::activated()
{
    m_view->focusText();
    updateFormatState();
    updateStatus();
}

QList<QPair<QString, QString>> WriterEditor::properties() const
{
    QList<QPair<QString, QString>> props = DocumentEditor::properties();
    const QLocale locale;
    props << qMakePair(tr("Sayfa"), locale.toString(m_view->pageCount()));
    props << qMakePair(tr("Sözcük"), locale.toString(TextOps::wordCount(m_doc->toPlainText())));
    props << qMakePair(tr("Karakter"), locale.toString(m_doc->characterCount() - 1));
    props << qMakePair(tr("Başlık"), documentTitleGuess());
    props << qMakePair(tr("Yazar"), m_author.isEmpty() ? AppSettings::instance().userName() : m_author);
    return props;
}
