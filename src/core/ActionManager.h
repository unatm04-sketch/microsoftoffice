#pragma once

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QMultiHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

class QAction;
class QWidget;

// Static description of a command. Texts are translated with the "Commands" context.
struct CommandSpec
{
    const char *id;
    const char *text;
    const char *icon = nullptr;
    const char *shortcut = nullptr; // portable text, several sequences separated by "; "
    bool checkable = false;
    const char *iconText = nullptr; // label of large ribbon buttons (may contain '\n')
};

// Central registry of commands. Modules declare their commands once; editors
// create QActions from the declarations, so shortcuts can be customised in one
// place and the command search can list every command of the suite.
class ActionManager : public QObject
{
    Q_OBJECT
public:
    struct Command
    {
        QString id;
        QString text;
        QString category;
        QString iconName;
        QString iconText;
        QList<QKeySequence> defaultShortcuts;
        bool checkable = false;
    };

    static ActionManager &instance();

    void declare(const QString &category, const QList<CommandSpec> &specs);
    bool isDeclared(const QString &id) const { return m_commands.contains(id); }
    Command command(const QString &id) const { return m_commands.value(id); }
    QList<Command> commands() const;
    QStringList categories() const { return m_categories; }

    // Creates an action for a declared command. The action is added to `owner`
    // and its shortcut is active while the focus is inside `owner`.
    QAction *create(const QString &id, QWidget *owner,
                    Qt::ShortcutContext context = Qt::WidgetWithChildrenShortcut);

    QList<QKeySequence> shortcuts(const QString &id) const;
    void setShortcuts(const QString &id, const QList<QKeySequence> &shortcuts);
    void resetShortcuts(const QString &id);
    void resetAll();
    // Returns the id of another command of the same category (or "Genel") using `sequence`.
    QString conflictingCommand(const QString &id, const QKeySequence &sequence) const;

    // Live action of command `id` that belongs to `owner` (or any live action if owner is null).
    QAction *liveAction(const QString &id, const QObject *owner = nullptr) const;

    static QString globalCategory();

signals:
    void shortcutsChanged(const QString &id);

private:
    ActionManager();
    void applyShortcuts(QAction *action, const QString &id) const;

    QHash<QString, Command> m_commands;
    QStringList m_order;
    QStringList m_categories;
    QHash<QString, QList<QKeySequence>> m_overrides;
    QMultiHash<QString, QPointer<QAction>> m_live;
};
