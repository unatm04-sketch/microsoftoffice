#include "core/widgets/Ribbon.h"

#include "core/Icons.h"
#include "core/ThemeManager.h"

#include <QAction>
#include <QButtonGroup>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kRibbonContentHeight = 98;

QToolButton *makeButton(QAction *action, QMenu *menu, bool split, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    if (menu) {
        button->setMenu(menu);
        button->setPopupMode(split ? QToolButton::MenuButtonPopup : QToolButton::InstantPopup);
    }
    return button;
}
} // namespace

// ---------------------------------------------------------------------------------
// RibbonGroup

RibbonGroup::RibbonGroup(const QString &title, QWidget *parent)
    : QFrame(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(5, 3, 5, 1);
    outer->setSpacing(1);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(2);
    m_grid->setVerticalSpacing(1);
    for (int r = 0; r < 3; ++r) {
        m_grid->setRowMinimumHeight(r, 24);
        m_grid->setRowStretch(r, 1);
    }
    outer->addLayout(m_grid, 1);

    m_titleRow = new QHBoxLayout;
    m_titleRow->setContentsMargins(0, 0, 0, 0);
    m_titleRow->setSpacing(0);
    m_title = new QLabel(title, this);
    m_title->setObjectName(QStringLiteral("RibbonGroupTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    m_titleRow->addWidget(m_title, 1);
    outer->addLayout(m_titleRow);
}

void RibbonGroup::newColumn()
{
    if (m_row != 0) {
        m_row = 0;
        ++m_column;
    }
}

QToolButton *RibbonGroup::addLargeAction(QAction *action, QMenu *menu, bool split)
{
    newColumn();
    QToolButton *button = makeButton(action, menu, split, this);
    button->setObjectName(QStringLiteral("RibbonLarge"));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(32, 32));
    button->setMinimumWidth(48);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_grid->addWidget(button, 0, m_column, 3, 1);
    ++m_column;
    return button;
}

QToolButton *RibbonGroup::addSmallAction(QAction *action, QMenu *menu, bool split)
{
    QToolButton *button = makeButton(action, menu, split, this);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(16, 16));
    button->setFixedHeight(24);
    m_grid->addWidget(button, m_row, m_column, Qt::AlignLeft | Qt::AlignVCenter);
    if (++m_row == 3) {
        m_row = 0;
        ++m_column;
    }
    return button;
}

QToolButton *RibbonGroup::iconButton(QAction *action, QMenu *menu, bool split)
{
    QToolButton *button = makeButton(action, menu, split, nullptr);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(16, 16));
    button->setFixedHeight(24);
    button->setMinimumWidth(menu && split ? 36 : 26);
    return button;
}

void RibbonGroup::addRow(const QList<QWidget *> &widgets)
{
    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);
    for (QWidget *w : widgets)
        layout->addWidget(w);
    layout->addStretch(1);
    m_grid->addWidget(row, m_row, m_column, Qt::AlignVCenter);
    if (++m_row == 3) {
        m_row = 0;
        ++m_column;
    }
}

void RibbonGroup::addLargeWidget(QWidget *widget)
{
    newColumn();
    widget->setParent(this);
    m_grid->addWidget(widget, 0, m_column, 3, 1);
    ++m_column;
}

void RibbonGroup::addSeparator()
{
    newColumn();
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setStyleSheet(QStringLiteral("color: palette(midlight);"));
    m_grid->addWidget(line, 0, m_column, 3, 1);
    ++m_column;
}

void RibbonGroup::setLauncher(QAction *action)
{
    auto *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setIcon(Icons::icon(QStringLiteral("launcher")));
    button->setIconSize(QSize(10, 10));
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(16, 14);
    m_titleRow->addWidget(button, 0, Qt::AlignRight | Qt::AlignVCenter);
}

// ---------------------------------------------------------------------------------
// RibbonPage

RibbonPage::RibbonPage(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);
    m_layout->setSpacing(0);
    m_layout->addStretch(1);
}

RibbonGroup *RibbonPage::addGroup(const QString &title)
{
    auto *group = new RibbonGroup(title, this);
    m_layout->insertWidget(m_layout->count() - 1, group);
    return group;
}

void RibbonPage::setTabVisible(bool visible)
{
    if (m_tabVisible == visible)
        return;
    m_tabVisible = visible;
    emit tabVisibilityChanged(visible);
}

// ---------------------------------------------------------------------------------
// RibbonBar

RibbonBar::RibbonBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RibbonBar"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabRow = new QWidget(this);
    m_tabRow->setObjectName(QStringLiteral("RibbonTabRow"));
    m_tabRow->setAttribute(Qt::WA_StyledBackground, true);
    auto *rowLayout = new QHBoxLayout(m_tabRow);
    rowLayout->setContentsMargins(0, 0, 6, 0);
    rowLayout->setSpacing(0);

    m_fileButton = new QToolButton(m_tabRow);
    m_fileButton->setObjectName(QStringLiteral("RibbonFileTab"));
    m_fileButton->setText(tr("Dosya"));
    m_fileButton->setFocusPolicy(Qt::NoFocus);
    m_fileButton->setToolTip(tr("Dosya menüsü: yeni, aç, kaydet, yazdır, dışa aktar"));
    connect(m_fileButton, &QToolButton::clicked, this, &RibbonBar::fileTabClicked);
    rowLayout->addWidget(m_fileButton);

    m_tabsLayout = new QHBoxLayout;
    m_tabsLayout->setContentsMargins(4, 0, 0, 0);
    m_tabsLayout->setSpacing(0);
    rowLayout->addLayout(m_tabsLayout);
    rowLayout->addStretch(1);

    m_collapseButton = new QToolButton(m_tabRow);
    m_collapseButton->setObjectName(QStringLiteral("RibbonCollapse"));
    m_collapseButton->setFocusPolicy(Qt::NoFocus);
    m_collapseButton->setIcon(Icons::icon(QStringLiteral("chevron-up")));
    m_collapseButton->setIconSize(QSize(14, 14));
    m_collapseButton->setToolTip(tr("Şeridi daralt (Ctrl+F1)"));
    connect(m_collapseButton, &QToolButton::clicked, this, [this] { setCollapsed(!m_collapsed); });
    rowLayout->addWidget(m_collapseButton);
    layout->addWidget(m_tabRow);

    m_stack = new QStackedWidget(this);
    m_stack->setFixedHeight(kRibbonContentHeight);
    layout->addWidget(m_stack);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &RibbonBar::updateStyle);
}

RibbonPage *RibbonBar::currentPage() const
{
    const int index = m_stack->currentIndex();
    return (index >= 0 && index < m_pages.size()) ? m_pages.at(index) : nullptr;
}

void RibbonBar::setPages(const QList<RibbonPage *> &pages)
{
    if (RibbonPage *current = currentPage())
        m_lastTitle = current->title();

    for (QToolButton *button : std::as_const(m_tabButtons)) {
        m_buttonGroup->removeButton(button);
        delete button;
    }
    m_tabButtons.clear();
    for (RibbonPage *page : std::as_const(m_pages))
        disconnect(page, nullptr, this, nullptr);
    while (m_stack->count() > 0) {
        QWidget *wrapper = m_stack->widget(0);
        m_stack->removeWidget(wrapper);
        if (auto *area = qobject_cast<QScrollArea *>(wrapper)) {
            if (QWidget *page = area->takeWidget()) {
                page->hide();
                page->setParent(nullptr);
            }
        }
        delete wrapper;
    }

    m_pages = pages;
    for (int i = 0; i < m_pages.size(); ++i) {
        RibbonPage *page = m_pages.at(i);
        auto *area = new QScrollArea;
        area->setObjectName(QStringLiteral("RibbonScroll"));
        area->setFrameShape(QFrame::NoFrame);
        area->setWidgetResizable(true);
        area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        area->setWidget(page);
        page->show();
        m_stack->addWidget(area);

        auto *button = new QToolButton(m_tabRow);
        button->setObjectName(QStringLiteral("RibbonTab"));
        button->setText(page->title());
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->installEventFilter(this);
        button->setVisible(page->isTabVisible());
        m_buttonGroup->addButton(button, i);
        m_tabsLayout->addWidget(button);
        m_tabButtons << button;
        connect(button, &QToolButton::clicked, this, [this, i] {
            selectIndex(i);
            if (m_collapsed)
                setCollapsed(false);
        });
        connect(page, &RibbonPage::tabVisibilityChanged, this, [this, i](bool visible) {
            if (i >= m_tabButtons.size())
                return;
            m_tabButtons.at(i)->setVisible(visible);
            if (!visible && m_stack->currentIndex() == i)
                selectIndex(0);
        });
        connect(page, &RibbonPage::activateRequested, this, [this, i] { selectIndex(i); });
    }

    int index = 0;
    for (int i = 0; i < m_pages.size(); ++i) {
        if (m_pages.at(i)->title() == m_lastTitle && m_pages.at(i)->isTabVisible()) {
            index = i;
            break;
        }
    }
    if (!m_pages.isEmpty())
        selectIndex(index);
    m_stack->setVisible(!m_pages.isEmpty() && !m_collapsed);
    m_collapseButton->setVisible(!m_pages.isEmpty());
}

void RibbonBar::setCurrentPage(RibbonPage *page)
{
    const int index = m_pages.indexOf(page);
    if (index >= 0)
        selectIndex(index);
}

void RibbonBar::selectIndex(int index)
{
    if (index < 0 || index >= m_pages.size())
        return;
    m_stack->setCurrentIndex(index);
    if (QAbstractButton *button = m_buttonGroup->button(index))
        button->setChecked(true);
    m_lastTitle = m_pages.at(index)->title();
}

void RibbonBar::setAccentColor(const QColor &color)
{
    m_accent = color;
    updateStyle();
}

void RibbonBar::updateStyle()
{
    if (m_accent.isValid())
        setStyleSheet(ThemeManager::instance().ribbonStyleSheet(m_accent));
}

void RibbonBar::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;
    m_stack->setVisible(!collapsed && !m_pages.isEmpty());
    m_collapseButton->setIcon(Icons::icon(collapsed ? QStringLiteral("chevron-down") : QStringLiteral("chevron-up")));
    m_collapseButton->setToolTip(collapsed ? tr("Şeridi genişlet (Ctrl+F1)") : tr("Şeridi daralt (Ctrl+F1)"));
    emit collapsedChanged(collapsed);
}

void RibbonBar::setFileTabVisible(bool visible)
{
    m_fileButton->setVisible(visible);
}

bool RibbonBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick && m_tabButtons.contains(qobject_cast<QToolButton *>(watched))) {
        setCollapsed(!m_collapsed);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
