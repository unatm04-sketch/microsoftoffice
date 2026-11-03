#pragma once

#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QStringList>

class Backstage;
class CommandSearch;
class DocumentEditor;
class NavigationRail;
class QStackedWidget;
class QTabBar;
class QTimer;
class QToolButton;
class RibbonBar;
class StartPage;
class ZoomControl;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Declares the global commands (call once before creating the window).
    static void declareCommands();

    DocumentEditor *currentEditor() const;
    QList<DocumentEditor *> editors() const;

    DocumentEditor *newDocument(const QString &moduleId, const QString &templateId = QString());
    DocumentEditor *openFile(const QString &path);
    void openFiles(const QStringList &paths);
    DocumentEditor *openTool(const QString &moduleId);
    void showStartPage();
    void restoreSession();
    void checkRecovery();
    void bringToFront();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createActions();
    void createUi();
    QWidget *createHeader();
    void connectEditor(DocumentEditor *editor);
    void addTab(QWidget *page, const QIcon &icon, const QString &title);
    void addEditor(DocumentEditor *editor);
    QWidget *tabPage(int index) const;
    int indexOf(QWidget *page) const;
    bool closeTab(int index);
    bool maybeSave(DocumentEditor *editor);
    bool saveEditor(DocumentEditor *editor, bool forceDialog);
    void onCurrentTabChanged(int index);
    void updateTab(DocumentEditor *editor);
    void updateWindowTitle();
    void updateActions();
    void showTabMenu(const QPoint &pos);
    void showNewMenu();

    void openDialog();
    void print();
    void printPreview();
    void exportDocument(const QString &formatId);
    void composeMail(const QStringList &attachments, const QString &subject);
    void showBackstage(int page);
    void hideBackstage();
    void showSettings();
    void showShortcuts();
    void showAbout();
    void toggleFullScreen(bool on);
    void moduleRequested(const QString &moduleId);

    QString recoveryDirectory() const;
    void autoRecover();
    void clearRecovery();
    void restartRecoveryTimer();

    QHash<QString, QAction *> m_actions;
    NavigationRail *m_nav = nullptr;
    QStackedWidget *m_rootStack = nullptr;
    QWidget *m_mainPage = nullptr;
    QWidget *m_header = nullptr;
    QTabBar *m_tabs = nullptr;
    QToolButton *m_newTabButton = nullptr;
    CommandSearch *m_search = nullptr;
    RibbonBar *m_ribbon = nullptr;
    QStackedWidget *m_pages = nullptr;
    Backstage *m_backstage = nullptr;
    QStackedWidget *m_statusStack = nullptr;
    ZoomControl *m_zoom = nullptr;
    StartPage *m_startPage = nullptr;
    QHash<DocumentEditor *, QPointer<QWidget>> m_statusWidgets;
    QHash<QString, int> m_untitledCounters;
    QStringList m_closedFiles;
    QTimer *m_recoveryTimer = nullptr;
    Qt::WindowStates m_stateBeforeFullScreen;
};
