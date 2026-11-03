#include "Automation.h"

#include "MainWindow.h"

#include "core/ActionManager.h"
#include "core/DocumentEditor.h"
#include "core/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPixmap>
#include <QTimer>

Automation::Automation(MainWindow *window, const QString &script, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_steps(script.split(QLatin1Char(';'), Qt::SkipEmptyParts))
{
}

void Automation::start()
{
    QTimer::singleShot(500, this, &Automation::runNext);
}

void Automation::sendKey(int key, Qt::KeyboardModifiers modifiers, const QString &text)
{
    QWidget *target = QApplication::focusWidget();
    if (!target)
        target = m_window;
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
    // ShortcutOverride first, so that QAction shortcuts behave as with a real keyboard.
    QKeyEvent overrideEvent(QEvent::ShortcutOverride, key, modifiers, text);
    overrideEvent.ignore();
    QApplication::sendEvent(target, &overrideEvent);
    const QKeySequence sequence(QKeyCombination(modifiers, Qt::Key(key)));
    if (!overrideEvent.isAccepted() && modifiers != Qt::NoModifier) {
        for (QAction *a : m_window->findChildren<QAction *>()) {
            if (a->isEnabled() && a->shortcuts().contains(sequence)) {
                QWidget *owner = qobject_cast<QWidget *>(a->parent());
                if (owner && owner->isVisible() && (owner == m_window || owner->isAncestorOf(target) || owner == target)) {
                    a->trigger();
                    return;
                }
            }
        }
    }
    QApplication::sendEvent(target, &press);
    QApplication::sendEvent(target, &release);
}

void Automation::runNext()
{
    if (m_steps.isEmpty())
        return;
    const QString step = m_steps.takeFirst().trimmed();
    const QString kind = step.section(QLatin1Char(':'), 0, 0);
    const QString arg = step.section(QLatin1Char(':'), 1);
    int delay = 250;

    if (kind == QLatin1String("new")) {
        m_window->newDocument(arg.section(QLatin1Char(':'), 0, 0), arg.section(QLatin1Char(':'), 1));
    } else if (kind == QLatin1String("open")) {
        m_window->openFile(arg);
    } else if (kind == QLatin1String("cmd")) {
        DocumentEditor *e = m_window->currentEditor();
        QAction *a = ActionManager::instance().liveAction(arg, e);
        if (!a)
            a = ActionManager::instance().liveAction(arg, m_window);
        if (a)
            a->trigger();
        else
            qWarning("automation: command %s not found", qPrintable(arg));
    } else if (kind == QLatin1String("type")) {
        for (const QChar ch : arg) {
            if (ch == QLatin1Char('|'))
                sendKey(Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
            else
                sendKey(ch.isLetter() ? ch.toUpper().unicode() : ch.unicode(), Qt::NoModifier, QString(ch));
        }
    } else if (kind == QLatin1String("key")) {
        const QKeySequence seq(arg);
        if (!seq.isEmpty()) {
            const QKeyCombination combo = seq[0];
            sendKey(combo.key(), combo.keyboardModifiers(), QString());
        }
    } else if (kind == QLatin1String("theme")) {
        ThemeManager::instance().setMode(arg == QLatin1String("dark") ? ThemeMode::Dark : ThemeMode::Light);
    } else if (kind == QLatin1String("zoom")) {
        if (DocumentEditor *e = m_window->currentEditor())
            e->setZoomPercent(arg.toInt());
    } else if (kind == QLatin1String("wait")) {
        delay = arg.toInt();
    } else if (kind == QLatin1String("shot")) {
        m_window->grab().save(arg, "PNG");
    } else if (kind == QLatin1String("quit")) {
        m_window->setProperty("automationQuit", true);
        QApplication::exit(0);
        return;
    }
    QTimer::singleShot(delay, this, &Automation::runNext);
}
