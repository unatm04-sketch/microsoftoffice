#pragma once

#include <QHash>
#include <QWidget>

class QButtonGroup;
class QToolButton;

// Vertical application bar with one entry per module.
class NavigationRail : public QWidget
{
    Q_OBJECT
public:
    explicit NavigationRail(QWidget *parent = nullptr);

    // "home" or a module id.
    void setActive(const QString &id);

signals:
    void homeRequested();
    void moduleRequested(const QString &moduleId);
    void settingsRequested();
    void themeToggleRequested();

private:
    QToolButton *addButton(const QString &id, const QIcon &icon, const QString &text, const QString &toolTip);
    void updateThemeButton();

    QHash<QString, QToolButton *> m_buttons;
    QButtonGroup *m_group = nullptr;
    QToolButton *m_themeButton = nullptr;
};
