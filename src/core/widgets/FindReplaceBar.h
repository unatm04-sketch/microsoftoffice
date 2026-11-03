#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

// Inline find & replace bar shared by the text based editors.
class FindReplaceBar : public QWidget
{
    Q_OBJECT
public:
    explicit FindReplaceBar(QWidget *parent = nullptr);

    void showFind(const QString &initialText = {});
    void showReplace(const QString &initialText = {});

    QString text() const;
    QString replacementText() const;
    bool caseSensitive() const;
    bool wholeWords() const;
    bool regularExpression() const;

    // `current` is 1-based; 0 means "no current match".
    void setMatchInfo(int current, int total);
    void setReplaceEnabled(bool enabled);

signals:
    void searchChanged();
    void findNext();
    void findPrevious();
    void replaceOne();
    void replaceAll();
    void closed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setReplaceVisible(bool visible);

    QLineEdit *m_find = nullptr;
    QLineEdit *m_replace = nullptr;
    QToolButton *m_toggleReplace = nullptr;
    QToolButton *m_case = nullptr;
    QToolButton *m_word = nullptr;
    QToolButton *m_regex = nullptr;
    QToolButton *m_prev = nullptr;
    QToolButton *m_next = nullptr;
    QToolButton *m_close = nullptr;
    QPushButton *m_replaceButton = nullptr;
    QPushButton *m_replaceAllButton = nullptr;
    QLabel *m_info = nullptr;
    QWidget *m_replaceRow = nullptr;
    bool m_replaceEnabled = true;
};
