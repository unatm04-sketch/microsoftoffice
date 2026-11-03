#include "modules/writer/DocumentPrinter.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QDate>
#include <QFileInfo>
#include <QLocale>
#include <QPainter>
#include <QPdfWriter>
#include <QTextDocument>
#include <QTextFrame>

#include <memory>

namespace {

void drawHeaderFooter(QPainter *painter, const QSizeF &page, const PageSetup &setup, int pageNumber, int pageCount)
{
    auto expand = [&](QString text) {
        text.replace(QLatin1String("{page}"), QString::number(pageNumber));
        text.replace(QLatin1String("{pages}"), QString::number(pageCount));
        text.replace(QLatin1String("{date}"), QLocale().toString(QDate::currentDate(), QLocale::ShortFormat));
        return text;
    };
    const QMarginsF m = setup.marginsPx();
    QFont font(QStringLiteral("Calibri"));
    font.setPointSizeF(9);
    painter->setFont(font);
    painter->setPen(QColor(0x50, 0x50, 0x50));
    const qreal width = page.width() - m.left() - m.right();
    if (!setup.header.isEmpty())
        painter->drawText(QRectF(m.left(), 0, width, m.top()), Qt::AlignHCenter | Qt::AlignVCenter, expand(setup.header));
    if (!setup.footer.isEmpty())
        painter->drawText(QRectF(m.left(), page.height() - m.bottom(), width, m.bottom()),
                          Qt::AlignHCenter | Qt::AlignVCenter, expand(setup.footer));
}

} // namespace

namespace DocumentPrinter {

void paint(const QTextDocument *document, const PageSetup &setup, QPagedPaintDevice *device)
{
    std::unique_ptr<QTextDocument> doc(document->clone());
    const QSizeF size = setup.sizePx();
    const QMarginsF m = setup.marginsPx();
    QTextFrameFormat ff = doc->rootFrame()->frameFormat();
    ff.setLeftMargin(m.left());
    ff.setRightMargin(m.right());
    ff.setTopMargin(m.top());
    ff.setBottomMargin(m.bottom());
    ff.setPadding(0);
    ff.setBorder(0);
    doc->rootFrame()->setFrameFormat(ff);
    doc->setPageSize(size);

    QPainter painter(device);
    if (!painter.isActive())
        return;
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    const qreal sx = device->width() / size.width();
    const qreal sy = device->height() / size.height();
    const int pages = qMax(1, doc->pageCount());
    for (int i = 0; i < pages; ++i) {
        if (i > 0)
            device->newPage();
        painter.save();
        painter.scale(sx, sy);
        painter.translate(0, -i * size.height());
        const QRectF view(0, i * size.height(), size.width(), size.height());
        painter.setClipRect(view);
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = view;
        ctx.palette.setColor(QPalette::Text, Qt::black);
        doc->documentLayout()->draw(&painter, ctx);
        painter.restore();

        painter.save();
        painter.scale(sx, sy);
        drawHeaderFooter(&painter, size, setup, i + 1, pages);
        painter.restore();
    }
}

bool exportPdf(const QTextDocument *document, const PageSetup &setup, const QString &path, const QString &title,
               const QString &author, QString *errorMessage)
{
    {
        QPdfWriter writer(path);
        writer.setResolution(600);
        writer.setPageLayout(setup.pageLayout());
        writer.setTitle(title);
        writer.setAuthor(author);
        writer.setCreator(QStringLiteral("OmniOffice"));
        paint(document, setup, &writer);
    }
    if (!QFileInfo::exists(path)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("DocumentPrinter", "PDF dosyası oluşturulamadı.");
        return false;
    }
    return true;
}

} // namespace DocumentPrinter
