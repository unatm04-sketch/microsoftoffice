#include "core/widgets/FindReplaceBar.h"

#include "core/Icons.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QToolButton *makeToggle(const QString &text, const QString &tip, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumWidth(28);
    return b;
}

QToolButton *makeIcon(const QString &icon, const QString &tip, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setIcon(Icons::icon(icon));
    b->setIconSize(QSize(16, 16));
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
} // namespace

FindReplaceBar::FindReplaceBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FindBar"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 5, 8, 5);
    outer->setSpacing(4);

    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    m_toggleReplace = makeIcon(QStringLiteral("chevron-right"), tr("Değiştirmeyi göster/gizle"), this);
    connect(m_toggleReplace, &QToolButton::clicked, this, [this] { setReplaceVisible(!m_replaceRow->isVisible()); });
    row->addWidget(m_toggleReplace);

    m_find = new QLineEdit(this);
    m_find->setPlaceholderText(tr("Bul"));
    m_find->setClearButtonEnabled(true);
    m_find->setMinimumWidth(260);
    m_find->addAction(Icons::icon(QStringLiteral("search")), QLineEdit::LeadingPosition);
    m_find->installEventFilter(this);
    connect(m_find, &QLineEdit::textChanged, this, &FindReplaceBar::searchChanged);
    row->addWidget(m_find);

    m_case = makeToggle(QStringLiteral("Aa"), tr("Büyük/küçük harf duyarlı"), this);
    m_word = makeToggle(QStringLiteral("ab"), tr("Yalnızca tam sözcükler"), this);
    m_regex = makeToggle(QStringLiteral(".*"), tr("Normal ifade"), this);
    QFont underlined = m_word->font();
    underlined.setUnderline(true);
    m_word->setFont(underlined);
    for (QToolButton *t : {m_case, m_word, m_regex}) {
        connect(t, &QToolButton::toggled, this, &FindReplaceBar::searchChanged);
        row->addWidget(t);
    }

    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("Muted"));
    m_info->setMinimumWidth(90);
    row->addWidget(m_info);

    m_prev = makeIcon(QStringLiteral("arrow-up"), tr("Önceki (Shift+Enter)"), this);
    m_next = makeIcon(QStringLiteral("arrow-down"), tr("Sonraki (Enter)"), this);
    connect(m_prev, &QToolButton::clicked, this, &FindReplaceBar::findPrevious);
    connect(m_next, &QToolButton::clicked, this, &FindReplaceBar::findNext);
    row->addWidget(m_prev);
    row->addWidget(m_next);
    row->addStretch(1);

    m_close = makeIcon(QStringLiteral("close"), tr("Kapat (Esc)"), this);
    connect(m_close, &QToolButton::clicked, this, [this] {
        hide();
        emit closed();
    });
    row->addWidget(m_close);
    outer->addLayout(row);

    m_replaceRow = new QWidget(this);
    auto *replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(28, 0, 0, 0);
    replaceLayout->setSpacing(4);
    m_replace = new QLineEdit(m_replaceRow);
    m_replace->setPlaceholderText(tr("Değiştir"));
    m_replace->setMinimumWidth(260);
    m_replace->installEventFilter(this);
    replaceLayout->addWidget(m_replace);
    m_replaceButton = new QPushButton(tr("Değiştir"), m_replaceRow);
    m_replaceAllButton = new QPushButton(tr("Tümünü Değiştir"), m_replaceRow);
    connect(m_replaceButton, &QPushButton::clicked, this, &FindReplaceBar::replaceOne);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &FindReplaceBar::replaceAll);
    replaceLayout->addWidget(m_replaceButton);
    replaceLayout->addWidget(m_replaceAllButton);
    replaceLayout->addStretch(1);
    outer->addWidget(m_replaceRow);

    setReplaceVisible(false);
    hide();
}

void FindReplaceBar::showFind(const QString &initialText)
{
    show();
    if (!initialText.isEmpty() && !initialText.contains(QChar::ParagraphSeparator))
        m_find->setText(initialText);
    m_find->setFocus();
    m_find->selectAll();
    emit searchChanged();
}

void FindReplaceBar::showReplace(const QString &initialText)
{
    showFind(initialText);
    if (m_replaceEnabled)
        setReplaceVisible(true);
}

QString FindReplaceBar::text() const { return m_find->text(); }
QString FindReplaceBar::replacementText() const { return m_replace->text(); }
bool FindReplaceBar::caseSensitive() const { return m_case->isChecked(); }
bool FindReplaceBar::wholeWords() const { return m_word->isChecked(); }
bool FindReplaceBar::regularExpression() const { return m_regex->isChecked(); }

void FindReplaceBar::setMatchInfo(int current, int total)
{
    if (m_find->text().isEmpty())
        m_info->clear();
    else if (total == 0)
        m_info->setText(tr("Sonuç yok"));
    else if (current > 0)
        m_info->setText(tr("%1 / %2").arg(current).arg(total));
    else
        m_info->setText(tr("%1 sonuç").arg(total));
}

void FindReplaceBar::setReplaceEnabled(bool enabled)
{
    m_replaceEnabled = enabled;
    m_toggleReplace->setVisible(enabled);
    if (!enabled)
        setReplaceVisible(false);
}

void FindReplaceBar::setReplaceVisible(bool visible)
{
    m_replaceRow->setVisible(visible);
    m_toggleReplace->setIcon(Icons::icon(visible ? QStringLiteral("chevron-down") : QStringLiteral("chevron-right")));
}

bool FindReplaceBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && (watched == m_find || watched == m_replace)) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            hide();
            emit closed();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (watched == m_replace)
                emit replaceOne();
            else if (key->modifiers() & Qt::ShiftModifier)
                emit findPrevious();
            else
                emit findNext();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
