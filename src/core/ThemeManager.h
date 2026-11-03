#pragma once

#include <QColor>
#include <QObject>

class QWidget;

enum class ThemeMode { Light, Dark, System };

// Colour tokens of the active theme. Widgets that paint themselves read these
// instead of hard-coding colours, so they follow theme switches automatically.
struct ThemePalette
{
    bool dark = false;
    QColor window;       // application background (status bar, nav rail)
    QColor header;       // document tab strip
    QColor surface;      // ribbon, panels, active tab
    QColor surfaceAlt;   // side panels, headers
    QColor canvas;       // area behind pages and slides
    QColor border;
    QColor borderStrong;
    QColor text;
    QColor textMuted;
    QColor textDisabled;
    QColor hover;
    QColor pressed;
    QColor checked;
    QColor selection;    // item view selection background
    QColor icon;
    QColor iconDisabled;
    QColor input;        // line edits, lists, sheets
    QColor grid;         // spreadsheet grid lines
    QColor accent;       // application accent (focus, primary buttons)
    QColor scroll;
    QColor scrollHover;
    QColor danger;
    QColor success;
};

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    static ThemeManager &instance();

    // Reads the saved mode and applies it. Call once after QApplication exists.
    void initialize();

    ThemeMode mode() const { return m_mode; }
    void setMode(ThemeMode mode);
    void toggle();
    bool isDark() const { return m_palette.dark; }
    const ThemePalette &palette() const { return m_palette; }

    // Module accent adjusted for the current theme (lighter on dark backgrounds).
    QColor accentFor(const QColor &moduleAccent) const;
    QString ribbonStyleSheet(const QColor &moduleAccent) const;
    QString arrowImage() const;
    // Applies native window decorations such as the dark title bar on Windows.
    void decorateWindow(QWidget *window) const;

signals:
    void themeChanged();

private:
    ThemeManager();
    void apply();
    bool systemPrefersDark() const;
    QString buildStyleSheet() const;
    static ThemePalette makePalette(bool dark);

    ThemeMode m_mode = ThemeMode::System;
    ThemePalette m_palette;
    bool m_applying = false;
};
