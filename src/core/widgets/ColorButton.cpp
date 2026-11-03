#include "core/widgets/ColorButton.h"

#include "core/Icons.h"
#include "core/ThemeManager.h"

#include <QColorDialog>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <functional>

namespace {

QColor tinted(const QColor &c, qreal factor)
{
    // factor > 0 lightens towards white, factor < 0 darkens towards black.
    if (factor >= 0)
        return QColor::fromRgbF(c.redF() + (1 - c.redF()) * factor, c.greenF() + (1 - c.greenF()) * factor,
                                c.blueF() + (1 - c.blueF()) * factor);
    const qreal f = 1 + factor;
    return QColor::fromRgbF(c.redF() * f, c.greenF() * f, c.blueF() * f);
}

// Grid of colour swatches painted directly for compactness.
class SwatchGrid : public QWidget
{
public:
    SwatchGrid(const QList<QColor> &colors, int columns, std::function<void(const QColor &)> picked, QWidget *parent)
        : QWidget(parent), m_colors(colors), m_columns(columns), m_picked(std::move(picked))
    {
        setMouseTracking(true);
        const int rows = (colors.size() + columns - 1) / columns;
        setFixedSize(columns * kCell + 2, rows * kCell + 2);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const QColor frame = ThemeManager::instance().palette().borderStrong;
        for (int i = 0; i < m_colors.size(); ++i) {
            const QRect r = cellRect(i).adjusted(2, 2, -2, -2);
            p.fillRect(r, m_colors.at(i));
            p.setPen(i == m_hover ? ThemeManager::instance().palette().accent : frame);
            p.drawRect(r.adjusted(0, 0, -1, -1));
            if (i == m_hover) {
                p.drawRect(r.adjusted(-1, -1, 0, 0));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        const int index = indexAt(e->position().toPoint());
        if (index != m_hover) {
            m_hover = index;
            if (index >= 0)
                setToolTip(m_colors.at(index).name().toUpper());
            update();
        }
    }

    void leaveEvent(QEvent *) override
    {
        m_hover = -1;
        update();
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        const int index = indexAt(e->position().toPoint());
        if (index >= 0)
            m_picked(m_colors.at(index));
    }

private:
    static constexpr int kCell = 20;

    QRect cellRect(int i) const { return QRect(1 + (i % m_columns) * kCell, 1 + (i / m_columns) * kCell, kCell, kCell); }

    int indexAt(const QPoint &pos) const
    {
        for (int i = 0; i < m_colors.size(); ++i) {
            if (cellRect(i).contains(pos))
                return i;
        }
        return -1;
    }

    QList<QColor> m_colors;
    int m_columns;
    std::function<void(const QColor &)> m_picked;
    int m_hover = -1;
};

QList<QColor> themeColors()
{
    const QList<QColor> base = {QColor(0xFF, 0xFF, 0xFF), QColor(0x00, 0x00, 0x00), QColor(0xE7, 0xE6, 0xE6),
                                QColor(0x44, 0x54, 0x6A), QColor(0x44, 0x72, 0xC4), QColor(0xED, 0x7D, 0x31),
                                QColor(0xA5, 0xA5, 0xA5), QColor(0xFF, 0xC0, 0x00), QColor(0x5B, 0x9B, 0xD5),
                                QColor(0x70, 0xAD, 0x47)};
    QList<QColor> colors = base;
    const qreal shades[] = {0.8, 0.6, 0.4, -0.25, -0.5};
    for (qreal s : shades) {
        for (int i = 0; i < base.size(); ++i) {
            QColor c = base.at(i);
            // White and black get darker/lighter variants instead of no-ops.
            if (i == 0)
                c = tinted(QColor(0xFF, 0xFF, 0xFF), s > 0 ? -(1 - s) * 0.25 : s * 0.6);
            else if (i == 1)
                c = tinted(QColor(0x00, 0x00, 0x00), s > 0 ? s * 0.9 : (0.5 + s));
            else
                c = tinted(c, s);
            colors << c;
        }
    }
    return colors;
}

QList<QColor> standardColors()
{
    return {QColor(0xC0, 0x00, 0x00), QColor(0xFF, 0x00, 0x00), QColor(0xFF, 0xC0, 0x00), QColor(0xFF, 0xFF, 0x00),
            QColor(0x92, 0xD0, 0x50), QColor(0x00, 0xB0, 0x50), QColor(0x00, 0xB0, 0xF0), QColor(0x00, 0x70, 0xC0),
            QColor(0x00, 0x20, 0x60), QColor(0x70, 0x30, 0xA0)};
}

QList<QColor> highlightColors()
{
    return {QColor(0xFF, 0xFF, 0x00), QColor(0x00, 0xFF, 0x00), QColor(0x00, 0xFF, 0xFF), QColor(0xFF, 0x00, 0xFF),
            QColor(0x00, 0x00, 0xFF), QColor(0xFF, 0x00, 0x00), QColor(0x00, 0x00, 0x80), QColor(0x00, 0x80, 0x80),
            QColor(0x00, 0x80, 0x00), QColor(0x80, 0x00, 0x80), QColor(0x80, 0x00, 0x00), QColor(0x80, 0x80, 0x00),
            QColor(0x80, 0x80, 0x80), QColor(0xC0, 0xC0, 0xC0), QColor(0x00, 0x00, 0x00)};
}

} // namespace

ColorButton::ColorButton(const QString &iconName, Palette palette, QWidget *parent)
    : QToolButton(parent)
    , m_iconName(iconName)
    , m_palette(palette)
{
    setAutoRaise(true);
    setFocusPolicy(Qt::NoFocus);
    setPopupMode(QToolButton::MenuButtonPopup);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setIconSize(QSize(16, 16));
    setFixedHeight(24);
    setMinimumWidth(36);
    m_color = palette == Palette::Highlight ? QColor(0xFF, 0xFF, 0x00) : QColor(0xC0, 0x00, 0x00);

    rebuildMenu();
    updateIcon();
    connect(this, &QToolButton::clicked, this, [this] { emit colorPicked(m_color); });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &ColorButton::updateIcon);
}

void ColorButton::setColor(const QColor &color)
{
    m_color = color;
    updateIcon();
}

void ColorButton::setNoneText(const QString &text)
{
    m_noneText = text;
    rebuildMenu();
}

void ColorButton::rebuildMenu()
{
    auto *menu = new QMenu(this);
    auto *container = new QWidget(menu);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    auto pick = [this, menu](const QColor &c) {
        menu->close();
        setColor(c);
        emit colorPicked(c);
    };

    if (!m_noneText.isEmpty()) {
        auto *none = new QPushButton(m_noneText, container);
        none->setFlat(true);
        connect(none, &QPushButton::clicked, this, [pick] { pick(QColor()); });
        layout->addWidget(none);
    }
    if (m_palette == Palette::Highlight) {
        layout->addWidget(new SwatchGrid(highlightColors(), 5, pick, container));
    } else {
        auto *title = new QLabel(tr("Tema Renkleri"), container);
        title->setObjectName(QStringLiteral("Muted"));
        layout->addWidget(title);
        layout->addWidget(new SwatchGrid(themeColors(), 10, pick, container));
        auto *standard = new QLabel(tr("Standart Renkler"), container);
        standard->setObjectName(QStringLiteral("Muted"));
        layout->addWidget(standard);
        layout->addWidget(new SwatchGrid(standardColors(), 10, pick, container));
    }
    auto *more = new QPushButton(Icons::icon(QStringLiteral("palette")), tr("Diğer Renkler..."), container);
    more->setFlat(true);
    connect(more, &QPushButton::clicked, this, [this, menu, pick] {
        menu->close();
        const QColor c = QColorDialog::getColor(m_color.isValid() ? m_color : Qt::black, window(), tr("Renk Seç"));
        if (c.isValid())
            pick(c);
    });
    layout->addWidget(more);

    auto *action = new QWidgetAction(menu);
    action->setDefaultWidget(container);
    menu->addAction(action);

    if (QMenu *old = this->menu())
        old->deleteLater();
    setMenu(menu);
}

void ColorButton::updateIcon()
{
    QIcon icon;
    for (const qreal dpr : {1.0, 2.0}) {
        const int px = int(24 * dpr);
        QPixmap pm(px, px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.drawPixmap(QRect(0, 0, px, px), Icons::pixmap(m_iconName, 24, QColor(), dpr));
        const QRectF bar(3 * dpr, 19 * dpr, 18 * dpr, 4 * dpr);
        if (m_color.isValid()) {
            p.fillRect(bar, m_color);
        } else {
            p.setPen(QPen(ThemeManager::instance().palette().icon, dpr));
            p.drawRect(bar.adjusted(0.5, 0.5, -0.5, -0.5));
        }
        p.end();
        pm.setDevicePixelRatio(dpr);
        icon.addPixmap(pm);
    }
    setIcon(icon);
}
