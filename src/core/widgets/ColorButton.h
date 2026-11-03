#pragma once

#include <QColor>
#include <QToolButton>

// Split button that applies its current colour when clicked and offers an
// Office-like colour palette from the arrow. An invalid colour means
// "automatic" / "no colour".
class ColorButton : public QToolButton
{
    Q_OBJECT
public:
    enum class Palette { Theme, Highlight };

    explicit ColorButton(const QString &iconName, Palette palette = Palette::Theme, QWidget *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    // Adds an entry that picks an invalid colour (e.g. "Otomatik" or "Renk yok").
    void setNoneText(const QString &text);

signals:
    void colorPicked(const QColor &color);

private:
    void rebuildMenu();
    void updateIcon();

    QString m_iconName;
    Palette m_palette;
    QColor m_color;
    QString m_noneText;
};
