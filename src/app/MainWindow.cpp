#include "MainWindow.h"

#include "Backstage.h"
#include "CommandSearch.h"
#include "NavigationRail.h"
#include "SettingsDialog.h"
#include "StartPage.h"

#include "core/ActionManager.h"
#include "core/AppInfo.h"
#include "core/AppSettings.h"
#include "core/DocumentEditor.h"
#include "core/Icons.h"
#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/ThemeManager.h"
#include "core/Utils.h"
#include "core/widgets/Ribbon.h"
#include "core/widgets/ZoomControl.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

void MainWindow::declareCommands()
{
    ActionManager::instance().declare(ActionManager::globalCategory(), {
        {"app.new", QT_TRANSLATE_NOOP("Commands", "Yeni Belge"), "new", "Ctrl+N"},
        {"app.open", QT_TRANSLATE_NOOP("Commands", "Aç..."), "open", "Ctrl+O"},
        {"app.save", QT_TRANSLATE_NOOP("Commands", "Kaydet"), "save", "Ctrl+S"},
        {"app.saveAs", QT_TRANSLATE_NOOP("Commands", "Farklı Kaydet..."), "save-as", "F12; Ctrl+Shift+S"},
        {"app.print", QT_TRANSLATE_NOOP("Commands", "Yazdır..."), "print", "Ctrl+P"},
        {"app.printPreview", QT_TRANSLATE_NOOP("Commands", "Baskı Önizleme"), "eye", "Ctrl+F2"},
        {"app.exportPdf", QT_TRANSLATE_NOOP("Commands", "PDF Olarak Dışa Aktar..."), "pdf"},
        {"app.close", QT_TRANSLATE_NOOP("Commands", "Sekmeyi Kapat"), "close", "Ctrl+W; Ctrl+F4"},
        {"app.reopenClosed", QT_TRANSLATE_NOOP("Commands", "Kapatılan Sekmeyi Yeniden Aç"), "history", "Ctrl+Shift+T"},
        {"app.nextTab", QT_TRANSLATE_NOOP("Commands", "Sonraki Sekme"), "chevron-right", "Ctrl+Tab"},
        {"app.prevTab", QT_TRANSLATE_NOOP("Commands", "Önceki Sekme"), "chevron-left", "Ctrl+Shift+Tab"},
        {"app.undo", QT_TRANSLATE_NOOP("Commands", "Geri Al"), "undo", "Ctrl+Z"},
        {"app.redo", QT_TRANSLATE_NOOP("Commands", "Yinele"), "redo", "Ctrl+Y; Ctrl+Shift+Z"},
        {"app.find", QT_TRANSLATE_NOOP("Commands", "Bul"), "search", "Ctrl+F"},
        {"app.replace", QT_TRANSLATE_NOOP("Commands", "Bul ve Değiştir"), "replace", "Ctrl+H"},
        {"app.zoomIn", QT_TRANSLATE_NOOP("Commands", "Yakınlaştır"), "zoom-in", "Ctrl++; Ctrl+="},
        {"app.zoomOut", QT_TRANSLATE_NOOP("Commands", "Uzaklaştır"), "zoom-out", "Ctrl+-"},
        {"app.zoomReset", QT_TRANSLATE_NOOP("Commands", "Yakınlaştırmayı Sıfırla"), "actual-size", "Ctrl+0"},
        {"app.fullScreen", QT_TRANSLATE_NOOP("Commands", "Tam Ekran"), "fullscreen", "F11", true},
        {"app.toggleTheme", QT_TRANSLATE_NOOP("Commands", "Açık/Koyu Tema"), "moon", "Ctrl+Shift+L"},
        {"app.search", QT_TRANSLATE_NOOP("Commands", "Komut Ara"), "search", "Alt+Q; Ctrl+Shift+P"},
        {"app.settings", QT_TRANSLATE_NOOP("Commands", "Seçenekler"), "settings", "Ctrl+,"},
        {"app.toggleRibbon", QT_TRANSLATE_NOOP("Commands", "Şeridi Daralt/Genişlet"), "chevron-up", "Ctrl+F1"},
        {"app.home", QT_TRANSLATE_NOOP("Commands", "Giriş Sayfası"), "home", "Alt+Home"},
        {"app.backstage", QT_TRANSLATE_NOOP("Commands", "Dosya Menüsü"), "file", "Alt+F"},
        {"app.shortcuts", QT_TRANSLATE_NOOP("Commands", "Klavye Kısayolları"), "keyboard", "F1"},
        {"app.quit", QT_TRANSLATE_NOOP("Commands", "Çıkış"), "close", "Ctrl+Q"},
    });
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowIcon(Icons::logo());
    setAcceptDrops(true);
    setMinimumSize(960, 620);

    createActions();
    createUi();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        ThemeManager::instance().decorateWindow(this);
    });
    connect(&AppSettings::instance(), &AppSettings::settingsChanged, this, &MainWindow::restartRecoveryTimer);

    m_recoveryTimer = new QTimer(this);
    connect(m_recoveryTimer, &QTimer::timeout, this, &MainWindow::autoRecover);
    restartRecoveryTimer();

    QSettings &s = AppSettings::instance().settings();
    if (!restoreGeometry(s.value(QStringLiteral("window/geometry")).toByteArray()))
        resize(1380, 880);
    m_ribbon->setCollapsed(s.value(QStringLiteral("window/ribbonCollapsed"), false).toBool());
    updateActions();
    updateWindowTitle();
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------------
// Construction

void MainWindow::createActions()
{
    ActionManager &am = ActionManager::instance();
    auto make = [this, &am](const char *id, auto slot) {
        QAction *a = am.create(QString::fromLatin1(id), this, Qt::WindowShortcut);
        m_actions.insert(QString::fromLatin1(id), a);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    make("app.new", [this] {
        DocumentEditor *e = currentEditor();
        const bool sameType = e && e->module()->kind() == Module::Kind::Document && !e->module()->templates().isEmpty();
        newDocument(sameType ? e->module()->id() : QStringLiteral("writer"));
    });
    make("app.open", [this] { openDialog(); });
    make("app.save", [this] { saveEditor(currentEditor(), false); });
    make("app.saveAs", [this] { saveEditor(currentEditor(), true); });
    make("app.print", [this] { print(); });
    make("app.printPreview", [this] { printPreview(); });
    make("app.exportPdf", [this] { exportDocument(QStringLiteral("pdf")); });
    make("app.close", [this] {
        if (m_tabs->count() > 0)
            closeTab(m_tabs->currentIndex());
    });
    make("app.reopenClosed", [this] {
        while (!m_closedFiles.isEmpty()) {
            const QString path = m_closedFiles.takeLast();
            if (QFileInfo::exists(path)) {
                openFile(path);
                return;
            }
        }
        statusBar()->showMessage(tr("Yeniden açılacak sekme yok."), 3000);
    });
    make("app.nextTab", [this] {
        if (m_tabs->count() > 1)
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
    });
    make("app.prevTab", [this] {
        if (m_tabs->count() > 1)
            m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % m_tabs->count());
    });
    make("app.undo", [this] {
        if (DocumentEditor *e = currentEditor())
            e->undo();
    });
    make("app.redo", [this] {
        if (DocumentEditor *e = currentEditor())
            e->redo();
    });
    make("app.find", [this] {
        if (DocumentEditor *e = currentEditor())
            e->showFind();
    });
    make("app.replace", [this] {
        if (DocumentEditor *e = currentEditor())
            e->showReplace();
    });
    auto zoomBy = [this](int delta) {
        DocumentEditor *e = currentEditor();
        if (!e || !e->supportsZoom())
            return;
        const int current = e->zoomPercent();
        e->setZoomPercent(qBound(10, delta == 0 ? 100 : current + delta * (current >= 100 ? 25 : 10), 500));
        m_zoom->setZoom(e->zoomPercent());
    };
    make("app.zoomIn", [zoomBy] { zoomBy(1); });
    make("app.zoomOut", [zoomBy] { zoomBy(-1); });
    make("app.zoomReset", [zoomBy] { zoomBy(0); });
    make("app.fullScreen", [this](bool on) { toggleFullScreen(on); });
    make("app.toggleTheme", [] { ThemeManager::instance().toggle(); });
    make("app.search", [this] { m_search->activateSearch(); });
    make("app.settings", [this] { showSettings(); });
    make("app.toggleRibbon", [this] { m_ribbon->setCollapsed(!m_ribbon->isCollapsed()); });
    make("app.home", [this] { showStartPage(); });
    make("app.backstage", [this] { showBackstage(currentEditor() ? Backstage::InfoPage : Backstage::NewPage); });
    make("app.shortcuts", [this] { showShortcuts(); });
    make("app.quit", [this] { close(); });
}

void MainWindow::createUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_nav = new NavigationRail(central);
    connect(m_nav, &NavigationRail::homeRequested, this, &MainWindow::showStartPage);
    connect(m_nav, &NavigationRail::moduleRequested, this, &MainWindow::moduleRequested);
    connect(m_nav, &NavigationRail::settingsRequested, this, &MainWindow::showSettings);
    connect(m_nav, &NavigationRail::themeToggleRequested, this, [] { ThemeManager::instance().toggle(); });
    rootLayout->addWidget(m_nav);

    m_rootStack = new QStackedWidget(central);
    rootLayout->addWidget(m_rootStack, 1);

    m_mainPage = new QWidget;
    auto *mainLayout = new QVBoxLayout(m_mainPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(createHeader());
    m_ribbon = new RibbonBar(m_mainPage);
    connect(m_ribbon, &RibbonBar::fileTabClicked, this,
            [this] { showBackstage(currentEditor() ? Backstage::InfoPage : Backstage::NewPage); });
    mainLayout->addWidget(m_ribbon);
    m_pages = new QStackedWidget(m_mainPage);
    mainLayout->addWidget(m_pages, 1);
    m_rootStack->addWidget(m_mainPage);

    m_backstage = new Backstage(this);
    m_rootStack->addWidget(m_backstage);
    connect(m_backstage, &Backstage::backRequested, this, &MainWindow::hideBackstage);
    connect(m_backstage, &Backstage::newRequested, this, [this](const QString &m, const QString &t) {
        hideBackstage();
        newDocument(m, t);
    });
    connect(m_backstage, &Backstage::openRequested, this, [this](const QString &path) {
        hideBackstage();
        openFile(path);
    });
    connect(m_backstage, &Backstage::browseRequested, this, [this] {
        hideBackstage();
        openDialog();
    });
    connect(m_backstage, &Backstage::saveRequested, this, [this] {
        hideBackstage();
        saveEditor(currentEditor(), false);
    });
    connect(m_backstage, &Backstage::saveAsRequested, this, [this] {
        hideBackstage();
        saveEditor(currentEditor(), true);
    });
    connect(m_backstage, &Backstage::printRequested, this, [this] {
        hideBackstage();
        print();
    });
    connect(m_backstage, &Backstage::printPreviewRequested, this, [this] {
        hideBackstage();
        printPreview();
    });
    connect(m_backstage, &Backstage::exportRequested, this, [this](const QString &id) {
        hideBackstage();
        exportDocument(id);
    });
    connect(m_backstage, &Backstage::mailRequested, this, [this] {
        hideBackstage();
        if (DocumentEditor *e = currentEditor()) {
            if (QAction *send = ActionManager::instance().liveAction(e->module()->id() + QStringLiteral(".sendMail"), e))
                send->trigger();
            else if (!e->isUntitled())
                composeMail({e->filePath()}, e->documentTitle());
        }
    });
    connect(m_backstage, &Backstage::closeRequested, this, [this] {
        hideBackstage();
        if (m_tabs->count() > 0)
            closeTab(m_tabs->currentIndex());
    });
    connect(m_backstage, &Backstage::settingsRequested, this, [this] {
        hideBackstage();
        showSettings();
    });
    connect(m_backstage, &Backstage::aboutRequested, this, &MainWindow::showAbout);
    auto *escape = new QAction(m_backstage);
    escape->setShortcut(Qt::Key_Escape);
    escape->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QAction::triggered, this, &MainWindow::hideBackstage);
    m_backstage->addAction(escape);

    setCentralWidget(central);

    m_statusStack = new QStackedWidget(this);
    m_statusStack->addWidget(new QWidget);
    statusBar()->addWidget(m_statusStack, 1);
    m_zoom = new ZoomControl(this);
    connect(m_zoom, &ZoomControl::zoomRequested, this, [this](int percent) {
        if (DocumentEditor *e = currentEditor())
            e->setZoomPercent(percent);
    });
    statusBar()->addPermanentWidget(m_zoom);
    m_zoom->hide();

    m_startPage = new StartPage;
    m_startPage->hide();
    connect(m_startPage, &StartPage::newRequested, this, [this](const QString &m, const QString &t) { newDocument(m, t); });
    connect(m_startPage, &StartPage::openRequested, this, [this](const QString &path) { openFile(path); });
    connect(m_startPage, &StartPage::browseRequested, this, &MainWindow::openDialog);
    connect(m_startPage, &StartPage::toolRequested, this, [this](const QString &id) { openTool(id); });
}

QWidget *MainWindow::createHeader()
{
    m_header = new QWidget;
    m_header->setObjectName(QStringLiteral("AppHeader"));
    m_header->setAttribute(Qt::WA_StyledBackground, true);
    m_header->setFixedHeight(42);
    auto *layout = new QHBoxLayout(m_header);
    layout->setContentsMargins(10, 0, 8, 0);
    layout->setSpacing(2);

    auto *logo = new QLabel(m_header);
    logo->setPixmap(Icons::logo().pixmap(QSize(22, 22), devicePixelRatioF()));
    logo->setToolTip(AppInfo::displayName());
    layout->addWidget(logo);
    layout->addSpacing(6);

    for (const char *id : {"app.save", "app.undo", "app.redo"}) {
        auto *button = new QToolButton(m_header);
        button->setDefaultAction(m_actions.value(QString::fromLatin1(id)));
        button->setIconSize(QSize(16, 16));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(button);
    }
    layout->addSpacing(8);

    m_tabs = new QTabBar(m_header);
    m_tabs->setObjectName(QStringLiteral("DocumentTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setDrawBase(false);
    m_tabs->setMovable(true);
    m_tabs->setExpanding(false);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setFocusPolicy(Qt::NoFocus);
    m_tabs->setIconSize(QSize(16, 16));
    m_tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabs->setAcceptDrops(false);
    m_tabs->installEventFilter(this);
    connect(m_tabs, &QTabBar::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabs, &QTabBar::customContextMenuRequested, this, &MainWindow::showTabMenu);
    layout->addWidget(m_tabs, 1, Qt::AlignBottom);

    m_newTabButton = new QToolButton(m_header);
    m_newTabButton->setIcon(Icons::icon(QStringLiteral("plus")));
    m_newTabButton->setIconSize(QSize(16, 16));
    m_newTabButton->setToolTip(tr("Yeni sekme"));
    m_newTabButton->setFocusPolicy(Qt::NoFocus);
    m_newTabButton->setPopupMode(QToolButton::InstantPopup);
    auto *newMenu = new QMenu(m_newTabButton);
    for (Module *module : ModuleRegistry::instance().modules()) {
        const QString id = module->id();
        if (module->kind() == Module::Kind::Document && !module->templates().isEmpty())
            newMenu->addAction(ModuleRegistry::icon(module), tr("Yeni %1").arg(module->name()), this,
                               [this, id] { newDocument(id); });
    }
    newMenu->addSeparator();
    for (Module *module : ModuleRegistry::instance().modules()) {
        const QString id = module->id();
        if (module->kind() == Module::Kind::Tool)
            newMenu->addAction(ModuleRegistry::icon(module), module->name(), this, [this, id] { openTool(id); });
    }
    newMenu->addSeparator();
    newMenu->addAction(m_actions.value(QStringLiteral("app.open")));
    m_newTabButton->setMenu(newMenu);
    layout->addWidget(m_newTabButton);
    layout->addSpacing(12);

    m_search = new CommandSearch(m_header);
    connect(m_search, &CommandSearch::openFileRequested, this, [this](const QString &p) { openFile(p); });
    connect(m_search, &CommandSearch::finished, this, [this] {
        if (QWidget *page = m_pages->currentWidget())
            page->setFocus();
    });
    layout->addWidget(m_search);
    layout->addSpacing(4);

    auto *help = new QToolButton(m_header);
    help->setDefaultAction(m_actions.value(QStringLiteral("app.shortcuts")));
    help->setIcon(Icons::icon(QStringLiteral("help")));
    help->setIconSize(QSize(16, 16));
    help->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(help);
    return m_header;
}

// ---------------------------------------------------------------------------------
// Tabs

QWidget *MainWindow::tabPage(int index) const
{
    if (index < 0 || index >= m_tabs->count())
        return nullptr;
    return qobject_cast<QWidget *>(m_tabs->tabData(index).value<QObject *>());
}

int MainWindow::indexOf(QWidget *page) const
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (tabPage(i) == page)
            return i;
    }
    return -1;
}

DocumentEditor *MainWindow::currentEditor() const
{
    if (m_rootStack && m_pages)
        return qobject_cast<DocumentEditor *>(tabPage(m_tabs->currentIndex()));
    return nullptr;
}

QList<DocumentEditor *> MainWindow::editors() const
{
    QList<DocumentEditor *> result;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *e = qobject_cast<DocumentEditor *>(tabPage(i)))
            result << e;
    }
    return result;
}

void MainWindow::addTab(QWidget *page, const QIcon &icon, const QString &title)
{
    m_pages->addWidget(page);
    const int index = m_tabs->addTab(icon, title);
    m_tabs->setTabData(index, QVariant::fromValue(static_cast<QObject *>(page)));
    auto *close = new QToolButton(m_tabs);
    close->setObjectName(QStringLiteral("TabClose"));
    close->setIcon(Icons::icon(QStringLiteral("close")));
    close->setIconSize(QSize(12, 12));
    close->setAutoRaise(true);
    close->setFocusPolicy(Qt::NoFocus);
    close->setFixedSize(20, 20);
    close->setToolTip(tr("Kapat (Ctrl+W)"));
    connect(close, &QToolButton::clicked, this, [this, page] { closeTab(indexOf(page)); });
    m_tabs->setTabButton(index, QTabBar::RightSide, close);
    if (m_tabs->currentIndex() == index)
        onCurrentTabChanged(index);
    else
        m_tabs->setCurrentIndex(index);
}

void MainWindow::addEditor(DocumentEditor *editor)
{
    QWidget *previous = tabPage(m_tabs->currentIndex());
    auto *replaceable = qobject_cast<DocumentEditor *>(previous);
    if (replaceable
        && !(replaceable->isUntitled() && !replaceable->isModified() && !editor->isUntitled()
             && replaceable->module() == editor->module() && replaceable->module()->kind() == Module::Kind::Document)) {
        replaceable = nullptr;
    }
    connectEditor(editor);
    addTab(editor, editor->documentIcon(), editor->documentTitle());
    updateTab(editor);
    if (previous == m_startPage) {
        const int index = indexOf(m_startPage);
        if (index >= 0) {
            m_tabs->removeTab(index);
            m_pages->removeWidget(m_startPage);
            m_startPage->hide();
        }
    }
    if (replaceable)
        closeTab(indexOf(replaceable));
}

void MainWindow::connectEditor(DocumentEditor *editor)
{
    connect(editor, &DocumentEditor::modificationChanged, this, [this, editor] {
        updateTab(editor);
        updateWindowTitle();
        updateActions();
    });
    connect(editor, &DocumentEditor::titleChanged, this, [this, editor] {
        updateTab(editor);
        updateWindowTitle();
    });
    connect(editor, &DocumentEditor::undoRedoChanged, this, [this, editor] {
        if (editor == currentEditor())
            updateActions();
    });
    connect(editor, &DocumentEditor::zoomChanged, this, [this, editor](int percent) {
        if (editor == currentEditor())
            m_zoom->setZoom(percent);
    });
    connect(editor, &DocumentEditor::statusMessage, this, [this](const QString &message, int timeout) {
        if (message.isEmpty())
            statusBar()->clearMessage();
        else
            statusBar()->showMessage(message, timeout);
    });
    connect(editor, &DocumentEditor::openFileRequested, this, [this](const QString &path) { openFile(path); });
    connect(editor, &DocumentEditor::newDocumentRequested, this,
            [this](const QString &moduleId, const QString &templateId) { newDocument(moduleId, templateId); });
    connect(editor, &DocumentEditor::sendByMailRequested, this, &MainWindow::composeMail);
}

void MainWindow::onCurrentTabChanged(int index)
{
    QWidget *page = tabPage(index);
    if (!page) {
        m_ribbon->setPages({});
        m_ribbon->hide();
        m_statusStack->setCurrentIndex(0);
        m_zoom->hide();
        updateActions();
        updateWindowTitle();
        return;
    }
    m_pages->setCurrentWidget(page);
    if (auto *editor = qobject_cast<DocumentEditor *>(page)) {
        m_ribbon->setPages(editor->ribbonPages());
        m_ribbon->setAccentColor(editor->module()->accent());
        m_ribbon->show();
        QPointer<QWidget> &status = m_statusWidgets[editor];
        if (!status) {
            status = editor->statusWidget();
            if (status)
                m_statusStack->addWidget(status);
        }
        m_statusStack->setCurrentWidget(status ? status.data() : m_statusStack->widget(0));
        m_zoom->setVisible(editor->supportsZoom());
        m_zoom->setZoom(editor->zoomPercent());
        m_nav->setActive(editor->module()->id());
        m_search->setContext(editor, editor->module()->name(), this);
        editor->activated();
    } else {
        m_ribbon->setPages({});
        m_ribbon->hide();
        m_statusStack->setCurrentIndex(0);
        m_zoom->hide();
        m_nav->setActive(QStringLiteral("home"));
        m_search->setContext(nullptr, QString(), this);
    }
    updateActions();
    updateWindowTitle();
}

void MainWindow::updateTab(DocumentEditor *editor)
{
    const int index = indexOf(editor);
    if (index < 0)
        return;
    m_tabs->setTabText(index, editor->documentTitle() + (editor->isModified() ? QStringLiteral(" •") : QString()));
    m_tabs->setTabToolTip(index, editor->isUntitled() ? editor->documentTitle()
                                                      : QDir::toNativeSeparators(editor->filePath()));
}

void MainWindow::updateWindowTitle()
{
    QString title;
    if (DocumentEditor *e = currentEditor())
        title = e->documentTitle() + (e->isModified() ? QStringLiteral(" *") : QString());
    else
        title = tr("Giriş");
    setWindowTitle(title + QStringLiteral(" - ") + AppInfo::displayName());
}

void MainWindow::updateActions()
{
    DocumentEditor *e = currentEditor();
    auto enable = [this](const char *id, bool on) {
        if (QAction *a = m_actions.value(QString::fromLatin1(id)))
            a->setEnabled(on);
    };
    const bool saveable = e && e->canSave();
    enable("app.save", saveable);
    enable("app.saveAs", saveable);
    enable("app.print", e && e->canPrint());
    enable("app.printPreview", e && e->canPrint());
    bool pdf = false;
    if (e) {
        for (const ExportFormat &f : e->exportFormats())
            pdf = pdf || f.id == QLatin1String("pdf");
    }
    enable("app.exportPdf", pdf);
    enable("app.close", m_tabs->count() > 0);
    enable("app.undo", e && e->canUndo());
    enable("app.redo", e && e->canRedo());
    enable("app.find", e != nullptr);
    enable("app.replace", e != nullptr);
    enable("app.zoomIn", e && e->supportsZoom());
    enable("app.zoomOut", e && e->supportsZoom());
    enable("app.zoomReset", e && e->supportsZoom());
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_tabs) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::MiddleButton) {
                const int index = m_tabs->tabAt(me->position().toPoint());
                if (index >= 0)
                    closeTab(index);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_tabs->tabAt(me->position().toPoint()) < 0) {
                m_actions.value(QStringLiteral("app.new"))->trigger();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showTabMenu(const QPoint &pos)
{
    const int index = m_tabs->tabAt(pos);
    if (index < 0)
        return;
    QWidget *page = tabPage(index);
    auto *editor = qobject_cast<DocumentEditor *>(page);
    QMenu menu(this);
    menu.addAction(Icons::icon(QStringLiteral("close")), tr("Kapat"), this, [this, page] { closeTab(indexOf(page)); });
    menu.addAction(tr("Diğerlerini Kapat"), this, [this, page] {
        for (int i = m_tabs->count() - 1; i >= 0; --i) {
            if (tabPage(i) != page && !closeTab(i))
                return;
        }
    });
    menu.addAction(tr("Sağdakileri Kapat"), this, [this, page] {
        for (int i = m_tabs->count() - 1; i > indexOf(page); --i) {
            if (!closeTab(i))
                return;
        }
    });
    menu.addAction(tr("Tümünü Kapat"), this, [this] {
        for (int i = m_tabs->count() - 1; i >= 0; --i) {
            if (!closeTab(i))
                return;
        }
    });
    if (editor && !editor->isUntitled()) {
        const QString path = editor->filePath();
        menu.addSeparator();
        menu.addAction(Icons::icon(QStringLiteral("copy")), tr("Dosya Yolunu Kopyala"), this,
                       [path] { QApplication::clipboard()->setText(QDir::toNativeSeparators(path)); });
        menu.addAction(Icons::icon(QStringLiteral("folder")), tr("Dosya Konumunu Aç"), this,
                       [path] { Utils::revealInFileManager(path); });
    }
    menu.exec(m_tabs->mapToGlobal(pos));
}

bool MainWindow::closeTab(int index)
{
    QWidget *page = tabPage(index);
    if (!page)
        return false;
    if (auto *editor = qobject_cast<DocumentEditor *>(page)) {
        if (!maybeSave(editor))
            return false;
        if (!editor->isUntitled()) {
            m_closedFiles.removeAll(editor->filePath());
            m_closedFiles << editor->filePath();
        }
        if (editor == currentEditor())
            m_ribbon->setPages({});
        if (QWidget *status = m_statusWidgets.take(editor).data()) {
            m_statusStack->removeWidget(status);
            delete status;
        }
        m_tabs->removeTab(indexOf(editor));
        m_pages->removeWidget(editor);
        editor->deleteLater();
    } else {
        m_tabs->removeTab(index);
        m_pages->removeWidget(page);
        page->hide();
    }
    if (m_tabs->count() == 0)
        showStartPage();
    return true;
}

bool MainWindow::maybeSave(DocumentEditor *editor)
{
    if (!editor || !editor->isModified() || !editor->canSave())
        return true;
    m_tabs->setCurrentIndex(indexOf(editor));
    QMessageBox box(QMessageBox::Warning, AppInfo::displayName(),
                    tr("\"%1\" belgesindeki değişiklikleri kaydetmek istiyor musunuz?").arg(editor->documentTitle()),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.setInformativeText(tr("Kaydetmezseniz değişiklikleriniz kaybolur."));
    box.setDefaultButton(QMessageBox::Save);
    switch (box.exec()) {
    case QMessageBox::Save:
        return saveEditor(editor, false);
    case QMessageBox::Discard:
        return true;
    default:
        return false;
    }
}

bool MainWindow::saveEditor(DocumentEditor *editor, bool forceDialog)
{
    if (!editor || !editor->canSave())
        return false;
    QString path = editor->filePath();
    if (forceDialog || path.isEmpty() || !editor->canSaveInPlace()) {
        const QString dir = path.isEmpty() ? AppSettings::instance().lastDirectory() : QFileInfo(path).absolutePath();
        const QString base = path.isEmpty() ? editor->documentTitle() : QFileInfo(path).completeBaseName();
        QString selectedFilter;
        path = QFileDialog::getSaveFileName(this, tr("Farklı Kaydet"),
                                            QDir(dir).filePath(base + QLatin1Char('.') + editor->defaultExtension()),
                                            editor->saveFilter(), &selectedFilter);
        if (path.isEmpty())
            return false;
        path = Utils::ensureExtension(path, selectedFilter, editor->defaultExtension());
        AppSettings::instance().setLastDirectory(QFileInfo(path).absolutePath());
    }
    QString error;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = editor->saveFile(path, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::critical(this, tr("Kaydedilemedi"), error.isEmpty() ? tr("Dosya kaydedilemedi.") : error);
        return false;
    }
    AppSettings::instance().addRecentFile(path);
    statusBar()->showMessage(tr("Kaydedildi: %1").arg(QDir::toNativeSeparators(path)), 4000);
    return true;
}

// ---------------------------------------------------------------------------------
// Documents

DocumentEditor *MainWindow::newDocument(const QString &moduleId, const QString &templateId)
{
    hideBackstage();
    Module *module = ModuleRegistry::instance().module(moduleId);
    if (!module)
        return nullptr;
    if (module->kind() == Module::Kind::Tool)
        return openTool(moduleId);
    DocumentEditor *editor = module->createEditor(nullptr);
    const int number = ++m_untitledCounters[moduleId];
    editor->setUntitledName(QStringLiteral("%1 %2").arg(module->untitledBaseName()).arg(number));
    editor->newDocument();
    if (!templateId.isEmpty())
        editor->applyTemplate(templateId);
    addEditor(editor);
    return editor;
}

DocumentEditor *MainWindow::openFile(const QString &path)
{
    hideBackstage();
    const QFileInfo info(path);
    const QString absolute = info.absoluteFilePath();
    for (DocumentEditor *e : editors()) {
        if (!e->isUntitled() && QFileInfo(e->filePath()) == info) {
            m_tabs->setCurrentIndex(indexOf(e));
            return e;
        }
    }
    if (info.isDir()) {
        DocumentEditor *files = openTool(QStringLiteral("files"));
        if (files)
            files->handleRequest(QStringLiteral("navigate"), {{QStringLiteral("path"), absolute}});
        return files;
    }
    if (!info.exists()) {
        if (QMessageBox::question(this, tr("Dosya bulunamadı"),
                                  tr("\"%1\" bulunamadı. Son kullanılanlar listesinden kaldırılsın mı?")
                                      .arg(QDir::toNativeSeparators(absolute)))
            == QMessageBox::Yes)
            AppSettings::instance().removeRecentFile(absolute);
        return nullptr;
    }
    Module *module = ModuleRegistry::instance().moduleForFile(absolute);
    if (!module) {
        if (QMessageBox::question(this, AppInfo::displayName(),
                                  tr("\"%1\" türündeki dosyalar OmniOffice ile açılamıyor. Varsayılan uygulamayla açılsın mı?")
                                      .arg(info.suffix()))
            == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl::fromLocalFile(absolute));
        return nullptr;
    }
    DocumentEditor *editor = module->createEditor(nullptr);
    QString error;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = editor->openFile(absolute, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) {
        delete editor;
        QMessageBox::critical(this, tr("Dosya açılamadı"),
                              tr("\"%1\" açılamadı.\n\n%2").arg(info.fileName(), error));
        return nullptr;
    }
    addEditor(editor);
    AppSettings::instance().addRecentFile(absolute);
    AppSettings::instance().setLastDirectory(info.absolutePath());
    return editor;
}

void MainWindow::openFiles(const QStringList &paths)
{
    for (const QString &path : paths)
        openFile(path);
}

DocumentEditor *MainWindow::openTool(const QString &moduleId)
{
    hideBackstage();
    for (DocumentEditor *e : editors()) {
        if (e->module()->id() == moduleId) {
            m_tabs->setCurrentIndex(indexOf(e));
            return e;
        }
    }
    Module *module = ModuleRegistry::instance().module(moduleId);
    if (!module)
        return nullptr;
    DocumentEditor *editor = module->createEditor(nullptr);
    editor->setUntitledName(module->name());
    editor->newDocument();
    addEditor(editor);
    return editor;
}

void MainWindow::showStartPage()
{
    hideBackstage();
    const int index = indexOf(m_startPage);
    if (index >= 0)
        m_tabs->setCurrentIndex(index);
    else
        addTab(m_startPage, Icons::tile(QStringLiteral("g-home"), QColor(0x3B, 0x4C, 0x6B)), tr("Giriş"));
}

void MainWindow::openDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Aç"), AppSettings::instance().lastDirectory(),
                                                            ModuleRegistry::instance().openFilter());
    if (files.isEmpty())
        return;
    AppSettings::instance().setLastDirectory(QFileInfo(files.first()).absolutePath());
    openFiles(files);
}

void MainWindow::moduleRequested(const QString &moduleId)
{
    Module *module = ModuleRegistry::instance().module(moduleId);
    if (!module)
        return;
    if (module->kind() == Module::Kind::Tool) {
        openTool(moduleId);
    } else if (!module->templates().isEmpty()) {
        newDocument(moduleId);
    } else {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, tr("Aç"), AppSettings::instance().lastDirectory(),
            module->openFilter() + QStringLiteral(";;") + tr("Tüm dosyalar (*)"));
        openFiles(files);
    }
}

void MainWindow::restoreSession()
{
    if (!AppSettings::instance().restoreSession())
        return;
    const QStringList files = AppSettings::instance().settings().value(QStringLiteral("session/files")).toStringList();
    for (const QString &f : files) {
        if (QFileInfo::exists(f))
            openFile(f);
    }
}

void MainWindow::bringToFront()
{
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
}

// ---------------------------------------------------------------------------------
// Print & export

void MainWindow::print()
{
    DocumentEditor *editor = currentEditor();
    if (!editor || !editor->canPrint())
        return;
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(editor->documentTitle());
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Yazdır - %1").arg(editor->documentTitle()));
    if (dialog.exec() == QDialog::Accepted)
        editor->print(&printer);
}

void MainWindow::printPreview()
{
    DocumentEditor *editor = currentEditor();
    if (!editor || !editor->canPrint())
        return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(tr("Baskı Önizleme - %1").arg(editor->documentTitle()));
    preview.resize(1100, 820);
    connect(&preview, &QPrintPreviewDialog::paintRequested, editor, &DocumentEditor::print);
    preview.exec();
}

void MainWindow::exportDocument(const QString &formatId)
{
    DocumentEditor *editor = currentEditor();
    if (!editor)
        return;
    ExportFormat format;
    for (const ExportFormat &f : editor->exportFormats()) {
        if (f.id == formatId)
            format = f;
    }
    if (format.id.isEmpty()) {
        QMessageBox::information(this, tr("Dışa Aktar"), tr("Bu belge türü seçilen biçime aktarılamıyor."));
        return;
    }
    const QString base = editor->isUntitled() ? editor->documentTitle() : QFileInfo(editor->filePath()).completeBaseName();
    const QString dir = editor->isUntitled() ? AppSettings::instance().lastDirectory()
                                             : QFileInfo(editor->filePath()).absolutePath();
    QString path = QFileDialog::getSaveFileName(this, tr("Dışa Aktar - %1").arg(format.title),
                                                QDir(dir).filePath(base + QLatin1Char('.') + format.extension),
                                                format.filter);
    if (path.isEmpty())
        return;
    path = Utils::ensureExtension(path, format.filter, format.extension);
    QString error;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = editor->exportTo(format.id, path, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::critical(this, tr("Dışa aktarılamadı"), error.isEmpty() ? tr("Dosya oluşturulamadı.") : error);
        return;
    }
    QMessageBox box(QMessageBox::Information, tr("Dışa Aktar"),
                    tr("Belge dışa aktarıldı:\n%1").arg(QDir::toNativeSeparators(path)), QMessageBox::Ok, this);
    QPushButton *open = box.addButton(tr("Dosyayı Aç"), QMessageBox::ActionRole);
    QPushButton *reveal = box.addButton(tr("Klasörde Göster"), QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == open) {
        if (ModuleRegistry::instance().moduleForFile(path))
            openFile(path);
        else
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (box.clickedButton() == reveal) {
        Utils::revealInFileManager(path);
    }
}

void MainWindow::composeMail(const QStringList &attachments, const QString &subject)
{
    DocumentEditor *mail = openTool(QStringLiteral("mail"));
    const QVariantMap args{{QStringLiteral("attachments"), attachments}, {QStringLiteral("subject"), subject}};
    if (!mail || !mail->handleRequest(QStringLiteral("compose"), args))
        QMessageBox::information(this, tr("E-posta"), tr("E-posta modülü kullanılamıyor."));
}

// ---------------------------------------------------------------------------------
// Backstage, settings, help

void MainWindow::showBackstage(int page)
{
    m_backstage->setEditor(currentEditor());
    m_backstage->showPage(static_cast<Backstage::Page>(page));
    m_rootStack->setCurrentWidget(m_backstage);
    m_backstage->setFocus();
}

void MainWindow::hideBackstage()
{
    if (!m_rootStack || m_rootStack->currentWidget() == m_mainPage)
        return;
    m_rootStack->setCurrentWidget(m_mainPage);
    if (DocumentEditor *e = currentEditor())
        e->activated();
}

void MainWindow::showSettings()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::showShortcuts()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Klavye Kısayolları"));
    dialog.resize(560, 620);
    auto *tree = new QTreeWidget(&dialog);
    tree->setColumnCount(2);
    tree->setHeaderLabels({tr("Komut"), tr("Kısayol")});
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->setRootIsDecorated(true);
    ActionManager &am = ActionManager::instance();
    DocumentEditor *e = currentEditor();
    const QStringList categories = {ActionManager::globalCategory(), e ? e->module()->name() : QString()};
    for (const QString &category : categories) {
        if (category.isEmpty())
            continue;
        auto *parent = new QTreeWidgetItem(tree, {category});
        QFont f = parent->font(0);
        f.setBold(true);
        parent->setFont(0, f);
        for (const ActionManager::Command &cmd : am.commands()) {
            const QList<QKeySequence> keys = am.shortcuts(cmd.id);
            if (cmd.category != category || keys.isEmpty())
                continue;
            QString text = cmd.text;
            text.remove(QLatin1Char('&'));
            auto *item = new QTreeWidgetItem(parent, {text, QKeySequence::listToString(keys, QKeySequence::NativeText)});
            if (!cmd.iconName.isEmpty())
                item->setIcon(0, Icons::icon(cmd.iconName));
        }
    }
    tree->expandAll();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QPushButton *customize = buttons->addButton(tr("Özelleştir..."), QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(customize, &QPushButton::clicked, &dialog, [this, &dialog] {
        dialog.accept();
        SettingsDialog settings(this);
        settings.showPage(tr("Klavye Kısayolları"));
        settings.exec();
    });
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(tree);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("%1 Hakkında").arg(AppInfo::displayName()),
                       tr("<h3>%1 %2</h3>"
                          "<p>Belge, elektronik tablo, sunu, PDF, not, dosya ve e-posta araçlarını tek bir "
                          "arayüzde birleştiren hepsi bir arada ofis uygulaması.</p>"
                          "<p>Qt %3 ve C++20 ile geliştirilmiştir.</p>")
                           .arg(AppInfo::displayName(), AppInfo::version(), QString::fromLatin1(qVersion())));
}

void MainWindow::toggleFullScreen(bool on)
{
    if (on) {
        m_stateBeforeFullScreen = windowState();
        showFullScreen();
    } else {
        setWindowState(m_stateBeforeFullScreen & ~Qt::WindowFullScreen);
        show();
    }
}

// ---------------------------------------------------------------------------------
// Recovery

QString MainWindow::recoveryDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/recovery");
}

void MainWindow::restartRecoveryTimer()
{
    const int minutes = AppSettings::instance().autoRecoverMinutes();
    if (minutes <= 0)
        m_recoveryTimer->stop();
    else
        m_recoveryTimer->start(minutes * 60 * 1000);
}

void MainWindow::autoRecover()
{
    QDir dir(recoveryDirectory());
    dir.mkpath(QStringLiteral("."));
    QJsonArray entries;
    QStringList keep;
    for (DocumentEditor *e : editors()) {
        if (!e->isModified() || !e->canSave())
            continue;
        const QString file = e->saveRecoveryCopy(dir.absolutePath());
        if (file.isEmpty())
            continue;
        keep << QFileInfo(file).fileName();
        QJsonObject o;
        o.insert(QStringLiteral("module"), e->module()->id());
        o.insert(QStringLiteral("title"), e->documentTitle());
        o.insert(QStringLiteral("original"), e->filePath());
        o.insert(QStringLiteral("file"), file);
        o.insert(QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate));
        entries.append(o);
    }
    for (const QFileInfo &info : dir.entryInfoList(QDir::Files)) {
        if (info.fileName() != QLatin1String("index.json") && !keep.contains(info.fileName()))
            QFile::remove(info.absoluteFilePath());
    }
    QSaveFile index(dir.filePath(QStringLiteral("index.json")));
    if (index.open(QIODevice::WriteOnly)) {
        index.write(QJsonDocument(entries).toJson());
        index.commit();
    }
    if (!entries.isEmpty())
        statusBar()->showMessage(tr("Otomatik kurtarma bilgisi kaydedildi."), 2500);
}

void MainWindow::clearRecovery()
{
    QDir(recoveryDirectory()).removeRecursively();
}

void MainWindow::checkRecovery()
{
    QFile file(recoveryDirectory() + QStringLiteral("/index.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray entries = QJsonDocument::fromJson(file.readAll()).array();
    file.close();
    if (entries.isEmpty()) {
        clearRecovery();
        return;
    }
    QStringList titles;
    for (const QJsonValue &v : entries)
        titles << QStringLiteral("• ") + v.toObject().value(QStringLiteral("title")).toString();
    const auto answer = QMessageBox::question(
        this, tr("Belge Kurtarma"),
        tr("OmniOffice beklenmedik şekilde kapanmış. Kaydedilmemiş şu belgeler kurtarılabilir:\n\n%1\n\nKurtarılsın mı?")
            .arg(titles.join(QLatin1Char('\n'))));
    if (answer == QMessageBox::Yes) {
        for (const QJsonValue &v : entries) {
            const QJsonObject o = v.toObject();
            Module *module = ModuleRegistry::instance().module(o.value(QStringLiteral("module")).toString());
            if (!module)
                continue;
            DocumentEditor *editor = module->createEditor(nullptr);
            const QString original = o.value(QStringLiteral("original")).toString();
            QString error;
            if (!editor->restoreRecovered(o.value(QStringLiteral("file")).toString(), original, &error)) {
                delete editor;
                continue;
            }
            if (original.isEmpty())
                editor->setUntitledName(o.value(QStringLiteral("title")).toString() + tr(" (Kurtarıldı)"));
            addEditor(editor);
        }
    }
    clearRecovery();
}

// ---------------------------------------------------------------------------------
// Window events

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    ThemeManager::instance().decorateWindow(this);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    for (DocumentEditor *e : editors()) {
        if (!maybeSave(e)) {
            event->ignore();
            return;
        }
    }
    QSettings &s = AppSettings::instance().settings();
    QStringList session;
    for (DocumentEditor *e : editors()) {
        if (!e->isUntitled() && e->module()->kind() == Module::Kind::Document)
            session << e->filePath();
    }
    s.setValue(QStringLiteral("session/files"), session);
    s.setValue(QStringLiteral("window/geometry"), saveGeometry());
    s.setValue(QStringLiteral("window/ribbonCollapsed"), m_ribbon->isCollapsed());
    m_recoveryTimer->stop();
    clearRecovery();
    m_ribbon->setPages({});
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            files << url.toLocalFile();
    }
    openFiles(files);
    event->acceptProposedAction();
}
