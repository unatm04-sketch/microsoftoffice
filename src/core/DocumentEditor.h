#pragma once

#include <QIcon>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

class Module;
class QPrinter;
class RibbonPage;

struct ExportFormat
{
    QString id;        // "pdf", "odt", ...
    QString title;     // "PDF belgesi"
    QString filter;    // "PDF belgesi (*.pdf)"
    QString extension; // "pdf"
};

// Base class of every tab hosted by the shell. Subclasses implement the
// document specific parts (file I/O, ribbon pages, status bar widget...).
class DocumentEditor : public QWidget
{
    Q_OBJECT
public:
    explicit DocumentEditor(Module *module, QWidget *parent = nullptr);
    ~DocumentEditor() override;

    Module *module() const { return m_module; }
    QString uid() const { return m_uid; }

    QString filePath() const { return m_filePath; }
    bool isUntitled() const { return m_filePath.isEmpty(); }
    QString documentTitle() const;
    void setUntitledName(const QString &name);
    bool isModified() const { return m_modified; }
    QIcon documentIcon() const;

    // Document life cycle -----------------------------------------------------
    virtual void newDocument() {}
    virtual bool applyTemplate(const QString &templateId);
    bool openFile(const QString &path, QString *errorMessage);
    bool saveFile(const QString &path, QString *errorMessage);
    // Loads a crash-recovery copy and marks the document as modified.
    bool restoreRecovered(const QString &recoveryFile, const QString &originalPath, QString *errorMessage);

    virtual bool canSave() const { return true; }
    virtual QString saveFilter() const { return {}; }
    virtual QString defaultExtension() const { return {}; }
    // False when the current file uses a format that can only be exported.
    virtual bool canSaveInPlace() const { return !isUntitled(); }
    virtual QList<ExportFormat> exportFormats() const { return {}; }
    virtual bool exportTo(const QString &formatId, const QString &path, QString *errorMessage);
    virtual bool canPrint() const { return false; }
    virtual void print(QPrinter *printer);
    // Writes a copy for crash recovery and returns its path (empty when unsupported).
    virtual QString saveRecoveryCopy(const QString &directory);

    // Commands routed from the shell ------------------------------------------
    virtual bool canUndo() const { return false; }
    virtual bool canRedo() const { return false; }
    virtual void undo() {}
    virtual void redo() {}
    virtual void showFind() {}
    virtual void showReplace() {}

    virtual bool supportsZoom() const { return false; }
    virtual int zoomPercent() const { return 100; }
    virtual void setZoomPercent(int percent) { Q_UNUSED(percent); }

    // Shell integration -------------------------------------------------------
    QList<RibbonPage *> ribbonPages();
    // Widget shown at the left of the status bar. Created on demand; the shell takes ownership.
    virtual QWidget *statusWidget() { return nullptr; }
    virtual QList<QPair<QString, QString>> properties() const;
    // Called whenever the editor becomes the active tab.
    virtual void activated() {}
    // Generic request from another module (e.g. "compose" for mail, "navigate" for files).
    virtual bool handleRequest(const QString &request, const QVariantMap &arguments)
    {
        Q_UNUSED(request);
        Q_UNUSED(arguments);
        return false;
    }

signals:
    void modificationChanged(bool modified);
    void titleChanged();
    void undoRedoChanged();
    void zoomChanged(int percent);
    void statusMessage(const QString &message, int timeoutMs);
    void openFileRequested(const QString &path);
    void newDocumentRequested(const QString &moduleId, const QString &templateId);
    void sendByMailRequested(const QStringList &attachments, const QString &subject);

protected:
    virtual QList<RibbonPage *> createRibbonPages() = 0;
    virtual bool readFile(const QString &path, QString *errorMessage) = 0;
    // Must not change the editor state: it is also used for recovery copies.
    virtual bool writeFile(const QString &path, QString *errorMessage) = 0;
    // Called after the document was saved to `path` by saveFile().
    virtual void fileSaved(const QString &path) { Q_UNUSED(path); }

    void setModified(bool modified);
    void setFilePath(const QString &path);
    void showMessage(const QString &message, int timeoutMs = 3000) { emit statusMessage(message, timeoutMs); }

private:
    Module *m_module;
    QString m_uid;
    QString m_filePath;
    QString m_untitledName;
    bool m_modified = false;
    bool m_ribbonCreated = false;
    QList<QPointer<RibbonPage>> m_ribbonPages;
};
