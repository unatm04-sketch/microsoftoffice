#include "core/ActionManager.h"

#include "core/AppSettings.h"
#include "core/Icons.h"

#include <QAction>
#include <QCoreApplication>
#include <QWidget>

namespace {
const char kShortcutGroup[] = "shortcuts";

QList<QKeySequence> parseShortcuts(const char *text)
{
    if (!text || !*text)
        return {};
    return QKeySequence::listFromString(QString::fromUtf8(text), QKeySequence::PortableText);
}
} // namespace

ActionManager &ActionManager::instance()
{
    static ActionManager manager;
    return manager;
}

ActionManager::ActionManager()
{
    QSettings &s = AppSettings::instance().settings();
    s.beginGroup(kShortcutGroup);
    const QStringList keys = s.childKeys();
    for (const QString &key : keys) {
        const QString id = QString(key).replace(QLatin1Char('|'), QLatin1Char('/'));
        m_overrides.insert(id, QKeySequence::listFromString(s.value(key).toString(), QKeySequence::PortableText));
    }
    s.endGroup();
}

QString ActionManager::globalCategory()
{
    return QCoreApplication::translate("Commands", "Genel");
}

void ActionManager::declare(const QString &category, const QList<CommandSpec> &specs)
{
    if (!m_categories.contains(category))
        m_categories << category;
    for (const CommandSpec &spec : specs) {
        Command cmd;
        cmd.id = QString::fromLatin1(spec.id);
        cmd.text = QCoreApplication::translate("Commands", spec.text);
        cmd.category = category;
        cmd.iconName = spec.icon ? QString::fromLatin1(spec.icon) : QString();
        cmd.iconText = spec.iconText ? QCoreApplication::translate("Commands", spec.iconText) : QString();
        cmd.defaultShortcuts = parseShortcuts(spec.shortcut);
        cmd.checkable = spec.checkable;
        if (!m_commands.contains(cmd.id))
            m_order << cmd.id;
        m_commands.insert(cmd.id, cmd);
    }
}

QList<ActionManager::Command> ActionManager::commands() const
{
    QList<Command> result;
    for (const QString &id : m_order)
        result << m_commands.value(id);
    return result;
}

QAction *ActionManager::create(const QString &id, QWidget *owner, Qt::ShortcutContext context)
{
    auto *action = new QAction(owner);
    const auto it = m_commands.constFind(id);
    if (it == m_commands.cend()) {
        qWarning("ActionManager: command '%s' was not declared", qPrintable(id));
        action->setText(id);
    } else {
        action->setText(it->text);
        if (!it->iconName.isEmpty())
            action->setIcon(Icons::icon(it->iconName));
        if (!it->iconText.isEmpty())
            action->setIconText(it->iconText);
        action->setCheckable(it->checkable);
    }
    action->setProperty("commandId", id);
    action->setShortcutContext(context);
    applyShortcuts(action, id);
    if (owner)
        owner->addAction(action);
    m_live.insert(id, QPointer<QAction>(action));
    return action;
}

void ActionManager::applyShortcuts(QAction *action, const QString &id) const
{
    const QList<QKeySequence> keys = shortcuts(id);
    action->setShortcuts(keys);
    QString tip = action->text();
    tip.remove(QLatin1Char('&'));
    if (!keys.isEmpty())
        tip += QStringLiteral(" (%1)").arg(keys.first().toString(QKeySequence::NativeText));
    action->setToolTip(tip);
}

QList<QKeySequence> ActionManager::shortcuts(const QString &id) const
{
    const auto it = m_overrides.constFind(id);
    if (it != m_overrides.cend())
        return *it;
    return m_commands.value(id).defaultShortcuts;
}

void ActionManager::setShortcuts(const QString &id, const QList<QKeySequence> &keys)
{
    m_overrides.insert(id, keys);
    QSettings &s = AppSettings::instance().settings();
    s.beginGroup(kShortcutGroup);
    s.setValue(QString(id).replace(QLatin1Char('/'), QLatin1Char('|')),
               QKeySequence::listToString(keys, QKeySequence::PortableText));
    s.endGroup();
    for (auto it = m_live.find(id); it != m_live.end() && it.key() == id; ++it) {
        if (*it)
            applyShortcuts(it->data(), id);
    }
    emit shortcutsChanged(id);
}

void ActionManager::resetShortcuts(const QString &id)
{
    m_overrides.remove(id);
    QSettings &s = AppSettings::instance().settings();
    s.beginGroup(kShortcutGroup);
    s.remove(QString(id).replace(QLatin1Char('/'), QLatin1Char('|')));
    s.endGroup();
    for (auto it = m_live.find(id); it != m_live.end() && it.key() == id; ++it) {
        if (*it)
            applyShortcuts(it->data(), id);
    }
    emit shortcutsChanged(id);
}

void ActionManager::resetAll()
{
    const QStringList ids = m_overrides.keys();
    for (const QString &id : ids)
        resetShortcuts(id);
}

QString ActionManager::conflictingCommand(const QString &id, const QKeySequence &sequence) const
{
    if (sequence.isEmpty())
        return {};
    const QString category = m_commands.value(id).category;
    for (const QString &other : m_order) {
        if (other == id)
            continue;
        const QString otherCategory = m_commands.value(other).category;
        const bool related = otherCategory == category || otherCategory == globalCategory()
                             || category == globalCategory();
        if (related && shortcuts(other).contains(sequence))
            return other;
    }
    return {};
}

QAction *ActionManager::liveAction(const QString &id, const QObject *owner) const
{
    QAction *fallback = nullptr;
    for (auto it = m_live.find(id); it != m_live.end() && it.key() == id; ++it) {
        QAction *a = it->data();
        if (!a)
            continue;
        if (!owner)
            return a;
        for (const QObject *o = a->parent(); o; o = o->parent()) {
            if (o == owner)
                return a;
        }
        if (!fallback)
            fallback = a;
    }
    return owner ? nullptr : fallback;
}
