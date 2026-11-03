#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Built-in SVG icon set. Line icons follow the active theme colour and are
// re-rendered automatically after a theme switch.
namespace Icons {

// Theme-aware line icon. A valid `color` pins the icon to that colour.
QIcon icon(const QString &name, const QColor &color = QColor());
// Rounded colour tile with a white glyph (module and file type icons).
QIcon tile(const QString &glyph, const QColor &background);
// Multi-colour application logo.
QIcon logo();

QPixmap pixmap(const QString &name, int size, const QColor &color = QColor(), qreal dpr = 1.0);
bool exists(const QString &name);

} // namespace Icons
