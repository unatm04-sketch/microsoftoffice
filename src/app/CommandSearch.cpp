#include "CommandSearch.h"

#include "core/ActionManager.h"
#include "core/AppSettings.h"
#include "core/Icons.h"
#include "core/ModuleRegistry.h"
#include "core/Utils.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCompleter>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QStandardItemModel>

namespace {
constexpr int kKindRole = Qt::UserRole + 1;
constexpr int kValueRole = Qt::UserRole + 2;
} // namespace

CommandSearch::CommandSearch(QWidget *parent)
    : QLineEdit(parent)
{
    setObjectName(QStringLiteral("HeaderSearch"));
    setPlaceholderText(tr("Ara: komutlar ve dosyalar (Alt+Q)"));
    setClearButtonEnabled(true);
    setFixedWidth(300);
    addAction(Icons::icon(QStringLiteral("search")), QLineEdit::LeadingPosition);

    m_model = new QStandardItemModel(this);
    m_completer = new QCompleter(m_model, this);
    m_completer->setWidget(this);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(12);
    m_completer->popup()->setIconSize(QSize(16, 16));

    connect(this, &QLineEdit::textEdited, this, &CommandSearch::refresh);
    connect(m_completer, QOverload<const QModelIndex &>::of(&QCompleter::activated), this,
            [this](const QModelIndex &index) { execute(index.row()); });
    connect(this, &QLineEdit::returnPressed, this, [this] {
        if (m_model->rowCount() > 0)
            execute(0);
    });
}

void CommandSearch::setContext(QObject *editor, const QString &category, QObject *window)
{
    m_editor = editor;
    m_category = category;
    m_window = window;
}

void CommandSearch::activateSearch()
{
    setFocus(Qt::ShortcutFocusReason);
    selectAll();
    refresh(text());
}

void CommandSearch::refresh(const QString &text)
{
    m_model->clear();
    const QString needle = text.trimmed();
    if (needle.isEmpty()) {
        m_completer->popup()->hide();
        return;
    }
    ActionManager &am = ActionManager::instance();
    int count = 0;
    for (const ActionManager::Command &cmd : am.commands()) {
        if (count >= 14)
            break;
        if (cmd.category != m_category && cmd.category != ActionManager::globalCategory())
            continue;
        if (!Utils::matchesSearch(cmd.text, needle))
            continue;
        QAction *action = am.liveAction(cmd.id, m_editor);
        if (!action)
            action = am.liveAction(cmd.id, m_window);
        if (!action || !action->isEnabled())
            continue;
        QString label = cmd.text;
        label.remove(QLatin1Char('&'));
        const QList<QKeySequence> keys = am.shortcuts(cmd.id);
        if (!keys.isEmpty())
            label += QStringLiteral("    ") + keys.first().toString(QKeySequence::NativeText);
        auto *item = new QStandardItem(action->icon(), label);
        item->setData(QStringLiteral("command"), kKindRole);
        item->setData(cmd.id, kValueRole);
        m_model->appendRow(item);
        ++count;
    }
    int files = 0;
    for (const QString &path : AppSettings::instance().recentFiles()) {
        if (files >= 6)
            break;
        if (!Utils::matchesSearch(QFileInfo(path).fileName(), needle))
            continue;
        auto *item = new QStandardItem(ModuleRegistry::icon(ModuleRegistry::instance().moduleForFile(path)),
                                       tr("Aç: %1").arg(QFileInfo(path).fileName()));
        item->setToolTip(QDir::toNativeSeparators(path));
        item->setData(QStringLiteral("file"), kKindRole);
        item->setData(path, kValueRole);
        m_model->appendRow(item);
        ++files;
    }
    if (m_model->rowCount() == 0) {
        auto *item = new QStandardItem(tr("Sonuç bulunamadı"));
        item->setEnabled(false);
        m_model->appendRow(item);
    }
    m_completer->complete(QRect(0, 0, width(), height()));
}

void CommandSearch::execute(int row)
{
    QStandardItem *item = m_model->item(row);
    if (!item || !item->isEnabled())
        return;
    const QString kind = item->data(kKindRole).toString();
    const QString value = item->data(kValueRole).toString();
    clear();
    m_model->clear();
    m_completer->popup()->hide();
    emit finished();
    if (kind == QLatin1String("file")) {
        emit openFileRequested(value);
        return;
    }
    ActionManager &am = ActionManager::instance();
    QAction *action = am.liveAction(value, m_editor);
    if (!action)
        action = am.liveAction(value, m_window);
    if (action && action->isEnabled())
        action->trigger();
}

void CommandSearch::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        clear();
        m_completer->popup()->hide();
        emit finished();
        return;
    }
    if ((event->key() == Qt::Key_Down || event->key() == Qt::Key_Up) && !m_completer->popup()->isVisible()) {
        refresh(text());
        return;
    }
    QLineEdit::keyPressEvent(event);
}
