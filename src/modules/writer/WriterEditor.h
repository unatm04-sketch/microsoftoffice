#pragma once

#include "core/DocumentEditor.h"
#include "formats/PageSetup.h"

#include <QPointer>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

class ColorButton;
class FindReplaceBar;
class PageView;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QKeyEvent;
class QSplitter;
class QTimer;
class QToolButton;
class QTreeWidget;
class RibbonPage;

// Text document that can resolve data: URLs and paths relative to the file.
class WriterDocument : public QTextDocument
{
    Q_OBJECT
public:
    using QTextDocument::QTextDocument;
    QString baseDirectory;

protected:
    QVariant loadResource(int type, const QUrl &name) override;
};

class WriterEditor : public DocumentEditor
{
    Q_OBJECT
public:
    explicit WriterEditor(Module *module, QWidget *parent = nullptr);
    ~WriterEditor() override;

    void newDocument() override;
    bool applyTemplate(const QString &templateId) override;
    QString saveFilter() const override;
    QString defaultExtension() const override { return QStringLiteral("docx"); }
    bool canSaveInPlace() const override;
    QList<ExportFormat> exportFormats() const override;
    bool exportTo(const QString &formatId, const QString &path, QString *errorMessage) override;
    bool canPrint() const override { return true; }
    void print(QPrinter *printer) override;

    bool canUndo() const override;
    bool canRedo() const override;
    void undo() override;
    void redo() override;
    void showFind() override;
    void showReplace() override;

    bool supportsZoom() const override { return true; }
    int zoomPercent() const override;
    void setZoomPercent(int percent) override;

    QWidget *statusWidget() override;
    QList<QPair<QString, QString>> properties() const override;
    void activated() override;

    QTextDocument *document() const { return m_doc; }

protected:
    QList<RibbonPage *> createRibbonPages() override;
    bool readFile(const QString &path, QString *errorMessage) override;
    bool writeFile(const QString &path, QString *errorMessage) override;
    void fileSaved(const QString &path) override;

private:
    void createActions();
    void connectView();
    QTextCursor textCursor() const;
    void applyCursor(const QTextCursor &cursor);
    void mergeFormat(const QTextCharFormat &format);
    void clearProperty(int property);
    void updateFormatState();
    void updateStatus();
    void scheduleStatus();
    bool writeFormat(const QString &path, const QString &format, QString *errorMessage);
    QString documentTitleGuess() const;
    qreal textWidth() const;

    // Find & replace
    void runSearch();
    void findStep(bool forward);
    void updateHighlights();
    void replaceCurrent();
    void replaceEverything();

    // Commands
    void paste(bool plainText);
    void copySelection(bool cut);
    void changeCaseCycle();
    void insertImagesFromDialog();
    void insertImageFiles(const QList<QUrl> &urls, int position);
    void insertLink();
    void openLink(const QString &href);
    void editImage(int position);
    void insertTableOfContents();
    void insertDateTime();
    void showContextMenu(const QPoint &screenPos);
    void showWordCount();
    void applyPageSetup(const PageSetup &setup);
    void showPageSetupDialog();
    void sendByMail();
    void tableCommand(const QString &command);
    void setCellColor(const QColor &color);
    void rebuildOutline();
    bool handleKey(QKeyEvent *event);
    void growFont(int direction);

    WriterDocument *m_doc = nullptr;
    PageView *m_view = nullptr;
    FindReplaceBar *m_find = nullptr;
    QSplitter *m_splitter = nullptr;
    QWidget *m_outlinePanel = nullptr;
    QTreeWidget *m_outline = nullptr;
    PageSetup m_setup;
    QString m_fileFormat;
    QString m_title;
    QString m_author;

    QHash<QString, QAction *> m_actions;
    QFontComboBox *m_fontCombo = nullptr;
    QComboBox *m_sizeCombo = nullptr;
    ColorButton *m_fontColor = nullptr;
    ColorButton *m_highlight = nullptr;
    ColorButton *m_cellColor = nullptr;
    QDoubleSpinBox *m_spaceBefore = nullptr;
    QDoubleSpinBox *m_spaceAfter = nullptr;
    QList<QToolButton *> m_styleButtons;
    QPointer<RibbonPage> m_tablePage;

    QPointer<QWidget> m_status;
    QToolButton *m_pageButton = nullptr;
    QToolButton *m_wordsButton = nullptr;

    QList<QTextCursor> m_matches;
    int m_currentMatch = -1;
    QTextCharFormat m_painterFormat;
    bool m_painterActive = false;

    QTimer *m_statusTimer = nullptr;
    QTimer *m_outlineTimer = nullptr;
    QTimer *m_searchTimer = nullptr;
};
