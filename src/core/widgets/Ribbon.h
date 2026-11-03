#pragma once

#include <QFrame>
#include <QList>
#include <QPointer>
#include <QWidget>

class QAction;
class QButtonGroup;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QMenu;
class QStackedWidget;
class QToolButton;

// A titled group of commands inside a ribbon page. Large buttons span the full
// height; small buttons and rows of widgets are stacked three per column.
class RibbonGroup : public QFrame
{
    Q_OBJECT
public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);

    QToolButton *addLargeAction(QAction *action, QMenu *menu = nullptr, bool split = false);
    QToolButton *addSmallAction(QAction *action, QMenu *menu = nullptr, bool split = false);
    void addRow(const QList<QWidget *> &widgets);
    void addLargeWidget(QWidget *widget);
    void addSeparator();
    void setLauncher(QAction *action);

    // Compact icon-only button for use inside rows.
    static QToolButton *iconButton(QAction *action, QMenu *menu = nullptr, bool split = false);

private:
    void newColumn();

    QGridLayout *m_grid = nullptr;
    QHBoxLayout *m_titleRow = nullptr;
    QLabel *m_title = nullptr;
    int m_column = 0;
    int m_row = 0;
};

class RibbonPage : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonPage(const QString &title, QWidget *parent = nullptr);

    QString title() const { return m_title; }
    RibbonGroup *addGroup(const QString &title);

    // Contextual pages (e.g. "Tablo") are only shown while relevant.
    bool isTabVisible() const { return m_tabVisible; }
    void setTabVisible(bool visible);

signals:
    void tabVisibilityChanged(bool visible);
    void activateRequested();

private:
    QString m_title;
    QHBoxLayout *m_layout = nullptr;
    bool m_tabVisible = true;
};

class RibbonBar : public QWidget
{
    Q_OBJECT
public:
    explicit RibbonBar(QWidget *parent = nullptr);

    // Shows the given pages. Pages are not deleted; the caller keeps ownership.
    void setPages(const QList<RibbonPage *> &pages);
    QList<RibbonPage *> pages() const { return m_pages; }
    RibbonPage *currentPage() const;
    void setCurrentPage(RibbonPage *page);

    void setAccentColor(const QColor &color);
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed);
    void setFileTabVisible(bool visible);

signals:
    void fileTabClicked();
    void collapsedChanged(bool collapsed);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void selectIndex(int index);
    void updateStyle();

    QWidget *m_tabRow = nullptr;
    QHBoxLayout *m_tabsLayout = nullptr;
    QToolButton *m_fileButton = nullptr;
    QToolButton *m_collapseButton = nullptr;
    QButtonGroup *m_buttonGroup = nullptr;
    QList<QToolButton *> m_tabButtons;
    QStackedWidget *m_stack = nullptr;
    QList<RibbonPage *> m_pages;
    QString m_lastTitle;
    QColor m_accent;
    bool m_collapsed = false;
};
