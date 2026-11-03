#pragma once

#include <QLineEdit>
#include <QPointer>

class QCompleter;
class QStandardItemModel;

// "What do you want to do?" box: searches the commands of the active editor,
// the global commands and the recent files.
class CommandSearch : public QLineEdit
{
    Q_OBJECT
public:
    explicit CommandSearch(QWidget *parent = nullptr);

    // Objects whose live actions are searched (active editor and main window).
    void setContext(QObject *editor, const QString &category, QObject *window);
    void activateSearch();

signals:
    void openFileRequested(const QString &path);
    void finished();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void refresh(const QString &text);
    void execute(int row);

    QCompleter *m_completer = nullptr;
    QStandardItemModel *m_model = nullptr;
    QPointer<QObject> m_editor;
    QPointer<QObject> m_window;
    QString m_category;
};
