#pragma once

#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QSizeF>
#include <QString>

// Page geometry of a text document. Margins are stored in millimetres.
struct PageSetup
{
    QPageSize pageSize = QPageSize(QPageSize::A4);
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QMarginsF margins = QMarginsF(25.4, 25.4, 25.4, 25.4); // left, top, right, bottom
    QString header;  // may contain {page}, {pages} and {date}
    QString footer;

    static constexpr double kPixelsPerMm = 96.0 / 25.4;

    QSizeF sizeMm() const
    {
        const QSizeF s = pageSize.size(QPageSize::Millimeter);
        return orientation == QPageLayout::Landscape ? s.transposed() : s;
    }
    QSizeF sizePx() const { return sizeMm() * kPixelsPerMm; }
    QMarginsF marginsPx() const { return margins * kPixelsPerMm; }
    QPageLayout pageLayout() const
    {
        return QPageLayout(pageSize, orientation, QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
    }

    bool operator==(const PageSetup &other) const
    {
        return pageSize.isEquivalentTo(other.pageSize) && orientation == other.orientation
               && margins == other.margins && header == other.header && footer == other.footer;
    }
    bool operator!=(const PageSetup &other) const { return !(*this == other); }
};
