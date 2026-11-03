#pragma once

#include <QPointer>
#include <QWidget>

class DocumentEditor;
class QButtonGroup;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class RecentFilesWidget;

// Office-like "File" view covering the window.
class Backstage : public QWidget
{
    Q_OBJECT
public:
    enum Page { NewPage, OpenPage, InfoPage, ExportPage, PrintPage, SharePage };

    explicit Backstage(QWidget *parent = nullptr);

    void setEditor(DocumentEditor *editor);
    void showPage(Page page);

signals:
    void backRequested();
    void newRequested(const QString &moduleId, const QString &templateId);
    void openRequested(const QString &path);
    void browseRequested();
    void saveRequested();
    void saveAsRequested();
    void printRequested();
    void printPreviewRequested();
    void exportRequested(const QString &formatId);
    void mailRequested();
    void closeRequested();
    void settingsRequested();
    void aboutRequested();

private:
    QWidget *createNavigation();
    QWidget *createNewPage();
    QWidget *createOpenPage();
    QWidget *pageFrame(const QString &title, QVBoxLayout **body);
    void rebuildInfo();
    void rebuildExport();
    void updateAccent();

    QPointer<DocumentEditor> m_editor;
    QWidget *m_nav = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QStackedWidget *m_stack = nullptr;
    QVBoxLayout *m_infoBody = nullptr;
    QVBoxLayout *m_exportBody = nullptr;
    QWidget *m_infoContent = nullptr;
    QWidget *m_exportContent = nullptr;
    QLabel *m_printInfo = nullptr;
    RecentFilesWidget *m_recent = nullptr;
    QList<QWidget *> m_documentOnly;
};
