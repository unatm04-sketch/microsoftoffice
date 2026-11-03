#include "Backstage.h"

#include "StartPage.h"

#include "core/DocumentEditor.h"
#include "core/Icons.h"
#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/ThemeManager.h"
#include "core/Utils.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDir>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QPushButton *navButton(const QString &icon, const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(parent);
    button->setFlat(true);
    button->setIcon(Icons::icon(icon, Qt::white));
    button->setIconSize(QSize(18, 18));
    button->setText(text);
    button->setFixedHeight(40);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QPushButton *bigButton(const QString &icon, const QString &title, const QString &subtitle, QWidget *parent)
{
    auto *button = new QPushButton(parent);
    button->setIcon(Icons::icon(icon));
    button->setIconSize(QSize(28, 28));
    button->setText(subtitle.isEmpty() ? title : title + QLatin1Char('\n') + subtitle);
    button->setMinimumHeight(58);
    button->setMinimumWidth(320);
    button->setStyleSheet(QStringLiteral("text-align: left; padding: 8px 14px;"));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

Backstage::Backstage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Backstage"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createNavigation());

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("BackstageContent"));
    layout->addWidget(m_stack, 1);

    m_stack->addWidget(createNewPage());
    m_stack->addWidget(createOpenPage());

    QWidget *info = pageFrame(tr("Bilgi"), &m_infoBody);
    m_stack->addWidget(info);

    QWidget *exportPage = pageFrame(tr("Dışa Aktar"), &m_exportBody);
    m_stack->addWidget(exportPage);

    QVBoxLayout *printBody = nullptr;
    QWidget *print = pageFrame(tr("Yazdır"), &printBody);
    QPushButton *printButton = bigButton(QStringLiteral("print"), tr("Yazdır"), tr("Yazıcı seçin ve belgeyi yazdırın (Ctrl+P)"), print);
    connect(printButton, &QPushButton::clicked, this, &Backstage::printRequested);
    QPushButton *previewButton = bigButton(QStringLiteral("eye"), tr("Baskı Önizleme"), tr("Sayfaları yazdırmadan önce görün"), print);
    connect(previewButton, &QPushButton::clicked, this, &Backstage::printPreviewRequested);
    m_printInfo = new QLabel(print);
    m_printInfo->setObjectName(QStringLiteral("Muted"));
    printBody->addWidget(printButton);
    printBody->addWidget(previewButton);
    printBody->addWidget(m_printInfo);
    printBody->addStretch(1);
    m_stack->addWidget(print);

    QVBoxLayout *shareBody = nullptr;
    QWidget *share = pageFrame(tr("Paylaş"), &shareBody);
    QPushButton *mailButton = bigButton(QStringLiteral("mail"), tr("E-postayla Gönder"),
                                        tr("Belgenin bir kopyasını e-posta eki olarak gönderin"), share);
    connect(mailButton, &QPushButton::clicked, this, &Backstage::mailRequested);
    shareBody->addWidget(mailButton);
    QPushButton *pdfButton = bigButton(QStringLiteral("pdf"), tr("PDF Olarak Kaydet"),
                                       tr("Biçimlendirmeyi koruyan, her yerde açılabilen bir kopya oluşturun"), share);
    connect(pdfButton, &QPushButton::clicked, this, [this] { emit exportRequested(QStringLiteral("pdf")); });
    shareBody->addWidget(pdfButton);
    shareBody->addStretch(1);
    m_stack->addWidget(share);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &Backstage::updateAccent);
}

QWidget *Backstage::createNavigation()
{
    m_nav = new QWidget(this);
    m_nav->setObjectName(QStringLiteral("BackstageNav"));
    m_nav->setAttribute(Qt::WA_StyledBackground, true);
    m_nav->setFixedWidth(220);
    auto *layout = new QVBoxLayout(m_nav);
    layout->setContentsMargins(0, 10, 0, 10);
    layout->setSpacing(0);

    QPushButton *back = navButton(QStringLiteral("arrow-left"), tr("Geri"), m_nav);
    connect(back, &QPushButton::clicked, this, &Backstage::backRequested);
    layout->addWidget(back);
    layout->addSpacing(10);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    const QList<QPair<QPair<QString, QString>, Page>> pages = {
        {{QStringLiteral("new"), tr("Yeni")}, NewPage},
        {{QStringLiteral("open"), tr("Aç")}, OpenPage},
        {{QStringLiteral("info"), tr("Bilgi")}, InfoPage},
        {{QStringLiteral("export"), tr("Dışa Aktar")}, ExportPage},
        {{QStringLiteral("print"), tr("Yazdır")}, PrintPage},
        {{QStringLiteral("share"), tr("Paylaş")}, SharePage}};
    for (const auto &p : pages) {
        QPushButton *button = navButton(p.first.first, p.first.second, m_nav);
        button->setCheckable(true);
        m_navGroup->addButton(button, p.second);
        const Page page = p.second;
        connect(button, &QPushButton::clicked, this, [this, page] { showPage(page); });
        layout->addWidget(button);
        if (page != NewPage && page != OpenPage)
            m_documentOnly << button;
    }
    layout->addSpacing(10);

    QPushButton *save = navButton(QStringLiteral("save"), tr("Kaydet"), m_nav);
    connect(save, &QPushButton::clicked, this, &Backstage::saveRequested);
    QPushButton *saveAs = navButton(QStringLiteral("save-as"), tr("Farklı Kaydet"), m_nav);
    connect(saveAs, &QPushButton::clicked, this, &Backstage::saveAsRequested);
    QPushButton *close = navButton(QStringLiteral("close"), tr("Kapat"), m_nav);
    connect(close, &QPushButton::clicked, this, &Backstage::closeRequested);
    for (QPushButton *b : {save, saveAs, close}) {
        layout->addWidget(b);
        m_documentOnly << b;
    }
    layout->addStretch(1);

    QPushButton *settings = navButton(QStringLiteral("settings"), tr("Seçenekler"), m_nav);
    connect(settings, &QPushButton::clicked, this, &Backstage::settingsRequested);
    QPushButton *about = navButton(QStringLiteral("info"), tr("Hakkında"), m_nav);
    connect(about, &QPushButton::clicked, this, &Backstage::aboutRequested);
    layout->addWidget(settings);
    layout->addWidget(about);
    return m_nav;
}

QWidget *Backstage::pageFrame(const QString &title, QVBoxLayout **body)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(48, 32, 48, 32);
    layout->setSpacing(12);
    auto *label = new QLabel(title, content);
    label->setObjectName(QStringLiteral("BackstageTitle"));
    layout->addWidget(label);
    layout->addSpacing(8);
    *body = layout;
    scroll->setWidget(content);
    return scroll;
}

QWidget *Backstage::createNewPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *page = pageFrame(tr("Yeni"), &body);
    for (Module *module : ModuleRegistry::instance().modules(Module::Kind::Document)) {
        const QList<DocumentTemplate> templates = module->templates();
        if (templates.isEmpty())
            continue;
        auto *title = new QLabel(module->name(), page);
        title->setObjectName(QStringLiteral("SectionTitle"));
        body->addWidget(title);
        auto *grid = new QGridLayout;
        grid->setSpacing(12);
        int i = 0;
        for (const DocumentTemplate &tpl : templates) {
            grid->addWidget(TemplateGallery::createTile(module, tpl, page, [this, module, tpl] {
                emit newRequested(module->id(), tpl.id);
            }), 0, i++);
        }
        grid->setColumnStretch(i, 1);
        body->addLayout(grid);
        body->addSpacing(8);
    }
    body->addStretch(1);
    return page;
}

QWidget *Backstage::createOpenPage()
{
    QVBoxLayout *body = nullptr;
    QWidget *page = pageFrame(tr("Aç"), &body);
    QPushButton *browse = bigButton(QStringLiteral("open"), tr("Gözat"), tr("Bilgisayarınızdaki bir dosyayı açın (Ctrl+O)"), page);
    browse->setMaximumWidth(420);
    connect(browse, &QPushButton::clicked, this, &Backstage::browseRequested);
    body->addWidget(browse);
    body->addSpacing(10);
    m_recent = new RecentFilesWidget(page);
    connect(m_recent, &RecentFilesWidget::openRequested, this, &Backstage::openRequested);
    body->addWidget(m_recent, 1);
    return page;
}

void Backstage::setEditor(DocumentEditor *editor)
{
    m_editor = editor;
    for (QWidget *w : std::as_const(m_documentOnly))
        w->setEnabled(editor != nullptr && (editor->canSave() || w == m_navGroup->button(InfoPage)));
    if (QAbstractButton *b = m_navGroup->button(PrintPage))
        b->setEnabled(editor && editor->canPrint());
    if (QAbstractButton *b = m_navGroup->button(ExportPage))
        b->setEnabled(editor && !editor->exportFormats().isEmpty());
    if (QAbstractButton *b = m_navGroup->button(SharePage))
        b->setEnabled(editor && editor->canSave());
    rebuildInfo();
    rebuildExport();
    if (m_printInfo && editor)
        m_printInfo->setText(tr("Belge: %1").arg(editor->documentTitle()));
    updateAccent();
}

void Backstage::showPage(Page page)
{
    if (QAbstractButton *button = m_navGroup->button(page)) {
        if (!button->isEnabled())
            page = NewPage;
    }
    if (page == OpenPage && m_recent)
        m_recent->refresh();
    m_stack->setCurrentIndex(page);
    if (QAbstractButton *button = m_navGroup->button(page))
        button->setChecked(true);
}

void Backstage::rebuildInfo()
{
    delete m_infoContent;
    m_infoContent = new QWidget;
    auto *layout = new QVBoxLayout(m_infoContent);
    layout->setContentsMargins(0, 0, 0, 0);
    if (!m_editor) {
        layout->addWidget(new QLabel(tr("Açık belge yok.")));
    } else {
        auto *name = new QLabel(QStringLiteral("<b>%1</b>").arg(m_editor->documentTitle().toHtmlEscaped()));
        name->setStyleSheet(QStringLiteral("font-size: 14pt;"));
        layout->addWidget(name);
        auto *form = new QFormLayout;
        form->setHorizontalSpacing(24);
        form->setVerticalSpacing(8);
        for (const auto &prop : m_editor->properties()) {
            auto *value = new QLabel(prop.second);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            auto *key = new QLabel(prop.first);
            key->setObjectName(QStringLiteral("Muted"));
            form->addRow(key, value);
        }
        layout->addLayout(form);
        if (!m_editor->isUntitled()) {
            const QString path = m_editor->filePath();
            auto *buttons = new QHBoxLayout;
            auto *reveal = new QPushButton(Icons::icon(QStringLiteral("folder")), tr("Dosya konumunu aç"));
            connect(reveal, &QPushButton::clicked, this, [path] { Utils::revealInFileManager(path); });
            auto *copy = new QPushButton(Icons::icon(QStringLiteral("copy")), tr("Yolu kopyala"));
            connect(copy, &QPushButton::clicked, this,
                    [path] { QApplication::clipboard()->setText(QDir::toNativeSeparators(path)); });
            buttons->addWidget(reveal);
            buttons->addWidget(copy);
            buttons->addStretch(1);
            layout->addSpacing(12);
            layout->addLayout(buttons);
        }
    }
    layout->addStretch(1);
    m_infoBody->addWidget(m_infoContent, 1);
}

void Backstage::rebuildExport()
{
    delete m_exportContent;
    m_exportContent = new QWidget;
    auto *layout = new QVBoxLayout(m_exportContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    if (m_editor) {
        for (const ExportFormat &format : m_editor->exportFormats()) {
            QString icon = QStringLiteral("export");
            if (format.id == QLatin1String("pdf"))
                icon = QStringLiteral("pdf");
            else if (format.id == QLatin1String("png") || format.id == QLatin1String("images"))
                icon = QStringLiteral("image");
            QPushButton *button = bigButton(icon, format.title, format.filter, m_exportContent);
            const QString id = format.id;
            connect(button, &QPushButton::clicked, this, [this, id] { emit exportRequested(id); });
            layout->addWidget(button);
        }
    }
    layout->addStretch(1);
    m_exportBody->addWidget(m_exportContent, 1);
}

void Backstage::updateAccent()
{
    QColor accent = m_editor && m_editor->module() ? m_editor->module()->accent() : QColor(0x3B, 0x4C, 0x6B);
    if (ThemeManager::instance().isDark())
        accent = accent.darker(120);
    m_nav->setStyleSheet(QStringLiteral(
                             "#BackstageNav { background-color: %1; }"
                             "#BackstageNav QPushButton { color: #FFFFFF; border: none; text-align: left; padding: 6px 22px; background: transparent; }"
                             "#BackstageNav QPushButton:hover { background-color: rgba(255,255,255,0.14); }"
                             "#BackstageNav QPushButton:checked { background-color: rgba(255,255,255,0.24); }"
                             "#BackstageNav QPushButton:disabled { color: rgba(255,255,255,0.45); }")
                             .arg(accent.name()));
}
