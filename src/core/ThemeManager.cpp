#include "core/ThemeManager.h"

#include "core/AppSettings.h"

#include <QApplication>
#include <QPalette>
#include <QStyleHints>
#include <QWidget>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

QString css(const QColor &c)
{
    if (c.alpha() == 255)
        return c.name();
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

QColor blend(const QColor &base, const QColor &over, qreal amount)
{
    return QColor::fromRgbF(base.redF() + (over.redF() - base.redF()) * amount,
                            base.greenF() + (over.greenF() - base.greenF()) * amount,
                            base.blueF() + (over.blueF() - base.blueF()) * amount);
}

// Application-wide style sheet. Tokens of the form @name are replaced by theme colours.
const char kStyleTemplate[] = R"(
QToolTip { color: @text; background-color: @surface; border: 1px solid @borderStrong; padding: 4px 6px; }

QMenu { background-color: @surface; border: 1px solid @borderStrong; padding: 4px 0px; }
QMenu::item { padding: 6px 26px 6px 30px; color: @text; background: transparent; }
QMenu::item:selected { background-color: @hover; }
QMenu::item:disabled { color: @textDisabled; }
QMenu::separator { height: 1px; background: @border; margin: 4px 10px; }
QMenu::icon { padding-left: 8px; }

QScrollBar:vertical { background: transparent; width: 12px; margin: 0px; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0px; }
QScrollBar::handle:vertical { background: @scroll; min-height: 32px; border-radius: 3px; margin: 2px 3px 2px 3px; }
QScrollBar::handle:horizontal { background: @scroll; min-width: 32px; border-radius: 3px; margin: 3px 2px 3px 2px; }
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: @scrollHover; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QSplitter::handle { background: @border; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }

QStatusBar { background-color: @window; color: @textMuted; border-top: 1px solid @border; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @textMuted; padding: 0px 6px; }
QStatusBar QToolButton { border: none; padding: 2px 6px; color: @textMuted; border-radius: 3px; background: transparent; }
QStatusBar QToolButton:hover { background-color: @hover; }

QTreeView, QListView, QTableView { background-color: @input; alternate-background-color: @surfaceAlt; border: 1px solid @border; selection-background-color: @selection; selection-color: @text; outline: 0; }
QHeaderView::section { background-color: @surfaceAlt; color: @textMuted; border: none; border-right: 1px solid @border; border-bottom: 1px solid @border; padding: 3px 6px; }

QPushButton[accent="true"] { background-color: @accent; color: #FFFFFF; border: 1px solid @accent; border-radius: 4px; padding: 6px 18px; }
QPushButton[accent="true"]:hover { background-color: @accentHover; border-color: @accentHover; }
QPushButton[accent="true"]:disabled { background-color: @border; border-color: @border; color: @textDisabled; }

#AppHeader { background-color: @header; }
#AppHeader QToolButton { border: none; border-radius: 4px; padding: 4px; background: transparent; }
#AppHeader QToolButton:hover { background-color: @hover; }
#AppHeader QToolButton:pressed { background-color: @pressed; }
QTabBar#DocumentTabs { background: transparent; }
QTabBar#DocumentTabs::tab { background: transparent; color: @textMuted; border: none; padding: 7px 4px 7px 10px; margin: 6px 1px 0px 1px; border-top-left-radius: 7px; border-top-right-radius: 7px; min-width: 100px; max-width: 230px; }
QTabBar#DocumentTabs::tab:selected { background: @surface; color: @text; }
QTabBar#DocumentTabs::tab:hover:!selected { background: @hover; }
QTabBar#DocumentTabs QToolButton { background: @header; border: none; }
QToolButton#TabClose { border: none; border-radius: 3px; padding: 1px; background: transparent; }
QToolButton#TabClose:hover { background-color: @pressed; }
QLineEdit#HeaderSearch { background-color: @surface; border: 1px solid @border; border-radius: 6px; padding: 4px 8px; color: @text; }
QLineEdit#HeaderSearch:focus { border: 1px solid @accent; }

#NavRail { background-color: @header; border-right: 1px solid @border; }
#NavRail QToolButton { border: none; border-left: 3px solid transparent; color: @textMuted; padding: 6px 0px 5px 0px; background: transparent; font-size: 8pt; }
#NavRail QToolButton:hover { background-color: @hover; color: @text; }
#NavRail QToolButton:checked { background-color: @surface; color: @text; border-left: 3px solid @accent; }

#RibbonBar { background-color: @surface; border-bottom: 1px solid @border; }
#RibbonTabRow { background-color: @surface; }
QToolButton#RibbonTab { border: none; border-bottom: 3px solid transparent; padding: 6px 11px 4px 11px; color: @text; background: transparent; }
QToolButton#RibbonTab:hover { background-color: @hover; }
QToolButton#RibbonCollapse { border: none; border-radius: 4px; padding: 3px; background: transparent; }
QToolButton#RibbonCollapse:hover { background-color: @hover; }
QScrollArea#RibbonScroll { background: transparent; border: none; }
RibbonGroup { border: none; border-right: 1px solid @border; background: transparent; }
QLabel#RibbonGroupTitle { color: @textMuted; font-size: 8pt; }
RibbonGroup QToolButton { border: 1px solid transparent; border-radius: 4px; background: transparent; color: @text; padding: 1px 3px; }
RibbonGroup QToolButton:hover { background-color: @hover; border-color: @hover; }
RibbonGroup QToolButton:pressed { background-color: @pressed; }
RibbonGroup QToolButton:disabled { color: @textDisabled; }
RibbonGroup QToolButton[popupMode="1"] { padding-right: 15px; }
RibbonGroup QToolButton[popupMode="2"] { padding-right: 13px; }
RibbonGroup QToolButton::menu-button { border: none; width: 13px; }
RibbonGroup QToolButton::menu-arrow { image: url(@arrow); width: 9px; height: 9px; }
RibbonGroup QToolButton::menu-indicator { image: url(@arrow); width: 9px; height: 9px; subcontrol-origin: padding; subcontrol-position: right center; }
RibbonGroup QToolButton#RibbonLarge::menu-indicator { subcontrol-position: bottom center; }
RibbonGroup QToolButton#RibbonLarge[popupMode="2"] { padding-right: 3px; padding-bottom: 9px; }
RibbonGroup QToolButton#StyleChip { border: 1px solid @border; background-color: @input; padding: 2px 6px; }
RibbonGroup QToolButton#StyleChip:hover { border-color: @borderStrong; }

#FindBar { background-color: @surfaceAlt; border-bottom: 1px solid @border; }
#SidePanel { background-color: @surfaceAlt; }
QLabel#PanelTitle { font-weight: 600; color: @text; padding: 6px 8px; }
QLabel#Muted { color: @textMuted; }

#StartPage { background-color: @window; }
QLabel#StartTitle { font-size: 22pt; color: @text; }
QLabel#SectionTitle { font-size: 12pt; font-weight: 600; color: @text; }
QToolButton#StartTile { border: 1px solid @border; border-radius: 8px; background-color: @surface; color: @text; padding: 8px; }
QToolButton#StartTile:hover { border: 1px solid @accent; background-color: @hover; }
QToolButton#ToolChip { border: 1px solid @border; border-radius: 16px; background-color: @surface; color: @text; padding: 6px 14px; }
QToolButton#ToolChip:hover { border: 1px solid @accent; }

#Backstage { background-color: @window; }
#BackstageContent { background-color: @window; }
QLabel#BackstageTitle { font-size: 20pt; color: @text; }
)";

} // namespace

ThemeManager &ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

ThemeManager::ThemeManager()
{
    m_palette = makePalette(false);
}

void ThemeManager::initialize()
{
    const int stored = AppSettings::instance().settings().value(QStringLiteral("appearance/theme"), 2).toInt();
    m_mode = static_cast<ThemeMode>(qBound(0, stored, 2));
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (!m_applying && m_mode == ThemeMode::System)
            apply();
    });
    apply();
}

void ThemeManager::setMode(ThemeMode mode)
{
    m_mode = mode;
    AppSettings::instance().settings().setValue(QStringLiteral("appearance/theme"), static_cast<int>(mode));
    apply();
}

void ThemeManager::toggle()
{
    setMode(isDark() ? ThemeMode::Light : ThemeMode::Dark);
}

bool ThemeManager::systemPrefersDark() const
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QColor ThemeManager::accentFor(const QColor &moduleAccent) const
{
    if (!m_palette.dark)
        return moduleAccent;
    QColor c = moduleAccent.toHsl();
    const qreal lightness = qMax(c.lightnessF(), 0.62);
    return QColor::fromHslF(c.hslHueF(), qMin(1.0, c.hslSaturationF() * 0.95), lightness).toRgb();
}

QString ThemeManager::arrowImage() const
{
    return m_palette.dark ? QStringLiteral(":/omni/ui/arrow-down-dark.svg")
                          : QStringLiteral(":/omni/ui/arrow-down-light.svg");
}

QString ThemeManager::ribbonStyleSheet(const QColor &moduleAccent) const
{
    const QColor accentText = accentFor(moduleAccent);
    const QColor fileBg = m_palette.dark ? moduleAccent.darker(105) : moduleAccent;
    const QColor tint = blend(m_palette.surface, accentText, m_palette.dark ? 0.28 : 0.16);
    const QColor tintBorder = blend(m_palette.surface, accentText, 0.45);
    return QStringLiteral(
               "QToolButton#RibbonFileTab { background-color: %1; color: #FFFFFF; border: none; padding: 6px 16px; }"
               "QToolButton#RibbonFileTab:hover { background-color: %2; }"
               "QToolButton#RibbonTab:checked { border-bottom: 3px solid %3; color: %3; }"
               "RibbonGroup QToolButton:checked { background-color: %4; border-color: %5; }")
        .arg(css(fileBg), css(fileBg.lighter(112)), css(accentText), css(tint), css(tintBorder));
}

void ThemeManager::decorateWindow(QWidget *window) const
{
#ifdef Q_OS_WIN
    if (!window || !window->isWindow())
        return;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = m_palette.dark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE is 20 on Windows 10 20H1 and later, 19 before.
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(window);
#endif
}

ThemePalette ThemeManager::makePalette(bool dark)
{
    ThemePalette p;
    p.dark = dark;
    if (!dark) {
        p.window = QColor(0xF3, 0xF3, 0xF3);
        p.header = QColor(0xE8, 0xE8, 0xE8);
        p.surface = QColor(0xFF, 0xFF, 0xFF);
        p.surfaceAlt = QColor(0xF7, 0xF7, 0xF7);
        p.canvas = QColor(0xE4, 0xE4, 0xE4);
        p.border = QColor(0xE0, 0xE0, 0xE0);
        p.borderStrong = QColor(0xC6, 0xC6, 0xC6);
        p.text = QColor(0x1F, 0x1F, 0x1F);
        p.textMuted = QColor(0x5E, 0x5E, 0x5E);
        p.textDisabled = QColor(0xA6, 0xA6, 0xA6);
        p.hover = QColor(0xEB, 0xEB, 0xEB);
        p.pressed = QColor(0xDC, 0xDC, 0xDC);
        p.checked = QColor(0xDD, 0xE6, 0xF2);
        p.selection = QColor(0xCC, 0xE0, 0xF5);
        p.icon = QColor(0x40, 0x40, 0x40);
        p.iconDisabled = QColor(0xB5, 0xB5, 0xB5);
        p.input = QColor(0xFF, 0xFF, 0xFF);
        p.grid = QColor(0xE1, 0xE1, 0xE1);
        p.accent = QColor(0x0F, 0x6C, 0xBD);
        p.scroll = QColor(0xC4, 0xC4, 0xC4);
        p.scrollHover = QColor(0x9E, 0x9E, 0x9E);
        p.danger = QColor(0xC4, 0x2B, 0x1C);
        p.success = QColor(0x10, 0x7C, 0x10);
    } else {
        p.window = QColor(0x20, 0x20, 0x20);
        p.header = QColor(0x19, 0x19, 0x19);
        p.surface = QColor(0x2B, 0x2B, 0x2B);
        p.surfaceAlt = QColor(0x26, 0x26, 0x26);
        p.canvas = QColor(0x16, 0x16, 0x16);
        p.border = QColor(0x3B, 0x3B, 0x3B);
        p.borderStrong = QColor(0x50, 0x50, 0x50);
        p.text = QColor(0xEC, 0xEC, 0xEC);
        p.textMuted = QColor(0xA8, 0xA8, 0xA8);
        p.textDisabled = QColor(0x6A, 0x6A, 0x6A);
        p.hover = QColor(0x38, 0x38, 0x38);
        p.pressed = QColor(0x45, 0x45, 0x45);
        p.checked = QColor(0x36, 0x44, 0x56);
        p.selection = QColor(0x2C, 0x4E, 0x72);
        p.icon = QColor(0xD6, 0xD6, 0xD6);
        p.iconDisabled = QColor(0x5E, 0x5E, 0x5E);
        p.input = QColor(0x1F, 0x1F, 0x1F);
        p.grid = QColor(0x3A, 0x3A, 0x3A);
        p.accent = QColor(0x3B, 0x8E, 0xEA);
        p.scroll = QColor(0x55, 0x55, 0x55);
        p.scrollHover = QColor(0x78, 0x78, 0x78);
        p.danger = QColor(0xFF, 0x6B, 0x5E);
        p.success = QColor(0x6C, 0xCB, 0x5F);
    }
    return p;
}

QString ThemeManager::buildStyleSheet() const
{
    const ThemePalette &p = m_palette;
    QList<QPair<QString, QString>> tokens = {
        {QStringLiteral("@window"), css(p.window)},
        {QStringLiteral("@header"), css(p.header)},
        {QStringLiteral("@surfaceAlt"), css(p.surfaceAlt)},
        {QStringLiteral("@surface"), css(p.surface)},
        {QStringLiteral("@canvas"), css(p.canvas)},
        {QStringLiteral("@borderStrong"), css(p.borderStrong)},
        {QStringLiteral("@border"), css(p.border)},
        {QStringLiteral("@textMuted"), css(p.textMuted)},
        {QStringLiteral("@textDisabled"), css(p.textDisabled)},
        {QStringLiteral("@text"), css(p.text)},
        {QStringLiteral("@hover"), css(p.hover)},
        {QStringLiteral("@pressed"), css(p.pressed)},
        {QStringLiteral("@checked"), css(p.checked)},
        {QStringLiteral("@selection"), css(p.selection)},
        {QStringLiteral("@input"), css(p.input)},
        {QStringLiteral("@accentHover"), css(p.accent.lighter(112))},
        {QStringLiteral("@accent"), css(p.accent)},
        {QStringLiteral("@scrollHover"), css(p.scrollHover)},
        {QStringLiteral("@scroll"), css(p.scroll)},
        {QStringLiteral("@arrow"), arrowImage()},
    };
    // Longer tokens first so that "@surfaceAlt" is not consumed by "@surface".
    std::stable_sort(tokens.begin(), tokens.end(), [](const auto &a, const auto &b) {
        return a.first.size() > b.first.size();
    });
    QString sheet = QString::fromUtf8(kStyleTemplate);
    for (const auto &t : std::as_const(tokens))
        sheet.replace(t.first, t.second);
    return sheet;
}

void ThemeManager::apply()
{
    m_applying = true;
    bool dark = false;
    switch (m_mode) {
    case ThemeMode::Light:
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        dark = false;
        break;
    case ThemeMode::Dark:
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
        dark = true;
        break;
    case ThemeMode::System:
        QGuiApplication::styleHints()->unsetColorScheme();
        dark = systemPrefersDark();
        break;
    }
    m_palette = makePalette(dark);
    const ThemePalette &p = m_palette;

    QPalette pal;
    pal.setColor(QPalette::Window, p.window);
    pal.setColor(QPalette::WindowText, p.text);
    pal.setColor(QPalette::Base, p.input);
    pal.setColor(QPalette::AlternateBase, p.surfaceAlt);
    pal.setColor(QPalette::ToolTipBase, p.surface);
    pal.setColor(QPalette::ToolTipText, p.text);
    pal.setColor(QPalette::PlaceholderText, p.textMuted);
    pal.setColor(QPalette::Text, p.text);
    pal.setColor(QPalette::Button, p.surface);
    pal.setColor(QPalette::ButtonText, p.text);
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Light, dark ? p.borderStrong : QColor(Qt::white));
    pal.setColor(QPalette::Midlight, p.border);
    pal.setColor(QPalette::Mid, p.borderStrong);
    pal.setColor(QPalette::Dark, dark ? QColor(0x14, 0x14, 0x14) : QColor(0xA0, 0xA0, 0xA0));
    pal.setColor(QPalette::Shadow, QColor(0, 0, 0, dark ? 160 : 90));
    pal.setColor(QPalette::Highlight, p.accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Link, p.accent);
    pal.setColor(QPalette::LinkVisited, p.accent.darker(120));
    pal.setColor(QPalette::Accent, p.accent);
    pal.setColor(QPalette::Disabled, QPalette::Text, p.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, p.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, p.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, p.border);
    QApplication::setPalette(pal);

    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
        app->setStyleSheet(buildStyleSheet());

    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (w->isWindow() && w->testAttribute(Qt::WA_WState_Created))
            decorateWindow(w);
    }
    m_applying = false;
    emit themeChanged();
}
