#include "NavigationRail.h"

#include "core/Icons.h"
#include "core/Module.h"
#include "core/ModuleRegistry.h"
#include "core/ThemeManager.h"

#include <QButtonGroup>
#include <QToolButton>
#include <QVBoxLayout>

NavigationRail::NavigationRail(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("NavRail"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(68);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(2);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    QToolButton *home = addButton(QStringLiteral("home"), Icons::tile(QStringLiteral("g-home"), QColor(0x3B, 0x4C, 0x6B)),
                                  tr("Giriş"), tr("Giriş sayfası"));
    connect(home, &QToolButton::clicked, this, &NavigationRail::homeRequested);
    layout->addWidget(home);

    for (Module *module : ModuleRegistry::instance().modules()) {
        const QString tip = module->kind() == Module::Kind::Document
                                ? tr("Yeni %1 (%2)").arg(module->name().toLower(), module->description())
                                : module->description();
        QToolButton *button = addButton(module->id(), ModuleRegistry::icon(module), module->name(), tip);
        const QString id = module->id();
        connect(button, &QToolButton::clicked, this, [this, id] { emit moduleRequested(id); });
        layout->addWidget(button);
    }
    layout->addStretch(1);

    m_themeButton = new QToolButton(this);
    m_themeButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    m_themeButton->setIconSize(QSize(20, 20));
    m_themeButton->setFixedSize(68, 52);
    m_themeButton->setFocusPolicy(Qt::NoFocus);
    connect(m_themeButton, &QToolButton::clicked, this, &NavigationRail::themeToggleRequested);
    layout->addWidget(m_themeButton);

    auto *settings = new QToolButton(this);
    settings->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    settings->setIcon(Icons::icon(QStringLiteral("settings")));
    settings->setIconSize(QSize(20, 20));
    settings->setText(tr("Ayarlar"));
    settings->setToolTip(tr("Seçenekler (Ctrl+,)"));
    settings->setFixedSize(68, 52);
    settings->setFocusPolicy(Qt::NoFocus);
    connect(settings, &QToolButton::clicked, this, &NavigationRail::settingsRequested);
    layout->addWidget(settings);

    updateThemeButton();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &NavigationRail::updateThemeButton);
}

QToolButton *NavigationRail::addButton(const QString &id, const QIcon &icon, const QString &text, const QString &toolTip)
{
    auto *button = new QToolButton(this);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIcon(icon);
    button->setIconSize(QSize(26, 26));
    button->setText(text);
    button->setToolTip(toolTip);
    button->setCheckable(true);
    button->setFixedSize(68, 58);
    button->setFocusPolicy(Qt::NoFocus);
    m_group->addButton(button);
    m_buttons.insert(id, button);
    return button;
}

void NavigationRail::setActive(const QString &id)
{
    if (QToolButton *button = m_buttons.value(id)) {
        button->setChecked(true);
    } else if (QAbstractButton *checked = m_group->checkedButton()) {
        m_group->setExclusive(false);
        checked->setChecked(false);
        m_group->setExclusive(true);
    }
}

void NavigationRail::updateThemeButton()
{
    const bool dark = ThemeManager::instance().isDark();
    m_themeButton->setIcon(Icons::icon(dark ? QStringLiteral("sun") : QStringLiteral("moon")));
    m_themeButton->setText(dark ? tr("Açık") : tr("Koyu"));
    m_themeButton->setToolTip(dark ? tr("Açık temaya geç (Ctrl+Shift+L)") : tr("Koyu temaya geç (Ctrl+Shift+L)"));
}
