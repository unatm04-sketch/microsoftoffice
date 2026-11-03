#include "modules/writer/PageView.h"

#include "core/TextOps.h"
#include "core/ThemeManager.h"

#include <QAbstractTextDocumentLayout>
#include <QDate>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>

#include <cmath>

namespace {
constexpr qreal kPageGap = 18.0;
constexpr qreal kSceneMargin = 28.0;
} // namespace

// ---------------------------------------------------------------------------------
// PageTextItem

PageTextItem::PageTextItem(QGraphicsItem *parent)
    : QGraphicsTextItem(parent)
{
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setAcceptDrops(true);
    setAcceptHoverEvents(true);
    setCursor(Qt::IBeamCursor);
}

void PageTextItem::setHighlights(const QList<QRectF> &rects, const QList<QRectF> &current)
{
    m_highlights = rects;
    m_current = current;
    update();
}

void PageTextItem::keyPressEvent(QKeyEvent *event)
{
    if (keyFilter && keyFilter(event)) {
        emit cursorMoved();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        emit pasteRequested();
        event->accept();
        return;
    }
    const bool enter = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
                       && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
    QGraphicsTextItem::keyPressEvent(event);
    if (enter)
        emit enterPressed();
    emit cursorMoved();
}

void PageTextItem::inputMethodEvent(QInputMethodEvent *event)
{
    QGraphicsTextItem::inputMethodEvent(event);
    emit cursorMoved();
}

void PageTextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        const QString href = document()->documentLayout()->anchorAt(event->pos());
        if (!href.isEmpty()) {
            emit linkClicked(href);
            event->accept();
            return;
        }
    }
    QGraphicsTextItem::mousePressEvent(event);
    emit cursorMoved();
}

void PageTextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsTextItem::mouseMoveEvent(event);
    emit cursorMoved();
}

void PageTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsTextItem::mouseReleaseEvent(event);
    emit cursorMoved();
    emit mouseReleased();
}

void PageTextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    const int pos = document()->documentLayout()->hitTest(event->pos(), Qt::ExactHit);
    if (pos >= 0) {
        for (int candidate : {pos, pos - 1}) {
            if (candidate < 0)
                continue;
            QTextCursor c(document());
            c.setPosition(candidate);
            c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            if (c.charFormat().isImageFormat()) {
                emit imageDoubleClicked(candidate);
                event->accept();
                return;
            }
        }
    }
    QGraphicsTextItem::mouseDoubleClickEvent(event);
    emit cursorMoved();
}

void PageTextItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QTextCursor c = textCursor();
    const int pos = document()->documentLayout()->hitTest(event->pos(), Qt::FuzzyHit);
    if (pos >= 0 && (!c.hasSelection() || pos < c.selectionStart() || pos > c.selectionEnd())) {
        c.setPosition(pos);
        setTextCursor(c);
        emit cursorMoved();
    }
    emit contextMenuRequested(event->screenPos());
    event->accept();
}

void PageTextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QString href = document()->documentLayout()->anchorAt(event->pos());
    setToolTip(href.isEmpty() ? QString()
                              : href + QLatin1Char('\n') + tr("Bağlantıyı açmak için Ctrl tuşuna basılı tutarak tıklayın"));
    QGraphicsTextItem::hoverMoveEvent(event);
}

void PageTextItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsTextItem::dragEnterEvent(event);
}

void PageTextItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QGraphicsTextItem::dragMoveEvent(event);
}

void PageTextItem::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const int pos = document()->documentLayout()->hitTest(event->pos(), Qt::FuzzyHit);
        emit filesDropped(event->mimeData()->urls(), pos);
        event->acceptProposedAction();
        return;
    }
    QGraphicsTextItem::dropEvent(event);
    emit cursorMoved();
}

void PageTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    for (const QRectF &r : std::as_const(m_highlights))
        painter->fillRect(r, QColor(255, 214, 0, 110));
    for (const QRectF &r : std::as_const(m_current))
        painter->fillRect(r, QColor(255, 140, 0, 150));
    QStyleOptionGraphicsItem opt(*option);
    opt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
    QGraphicsTextItem::paint(painter, &opt, widget);
}

// ---------------------------------------------------------------------------------
// PageView

PageView::PageView(QWidget *parent)
    : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    m_item = new PageTextItem;
    m_scene->addItem(m_item);

    setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    setFrameShape(QFrame::NoFrame);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFocusPolicy(Qt::StrongFocus);
    setCacheMode(QGraphicsView::CacheNone);
    setDarkPage(false);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { viewport()->update(); });
}

void PageView::setDocument(QTextDocument *document)
{
    m_item->setDocument(document);
    connect(document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this](const QSizeF &) { updatePages(); });
    connect(document, &QTextDocument::documentLayoutChanged, this, [this, document] {
        connect(document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
                [this](const QSizeF &) { updatePages(); });
    });
    applyPageSetup();
}

QTextDocument *PageView::document() const
{
    return m_item->document();
}

QTextCursor PageView::textCursor() const
{
    return m_item->textCursor();
}

void PageView::setTextCursor(const QTextCursor &cursor)
{
    m_item->setTextCursor(cursor);
}

void PageView::setPageSetup(const PageSetup &setup)
{
    m_setup = setup;
    applyPageSetup();
}

void PageView::applyPageSetup()
{
    QTextDocument *doc = document();
    if (!doc)
        return;
    const QSizeF size = m_setup.sizePx();
    const QMarginsF m = m_setup.marginsPx();
    const bool undo = doc->isUndoRedoEnabled();
    const bool modified = doc->isModified();
    // Frame format changes would otherwise end up on the undo stack.
    doc->setUndoRedoEnabled(false);
    QTextFrameFormat ff = doc->rootFrame()->frameFormat();
    ff.setLeftMargin(m.left());
    ff.setRightMargin(m.right());
    ff.setTopMargin(m.top());
    ff.setBottomMargin(m.bottom() + kPageGap);
    ff.setPadding(0);
    ff.setBorder(0);
    doc->rootFrame()->setFrameFormat(ff);
    doc->setPageSize(QSizeF(size.width(), size.height() + kPageGap));
    doc->setUndoRedoEnabled(undo);
    doc->setModified(modified);
    updatePages();
    applyFit();
}

qreal PageView::pageStride() const
{
    return m_setup.sizePx().height() + kPageGap;
}

void PageView::updatePages()
{
    QTextDocument *doc = document();
    const int pages = doc ? qMax(1, doc->pageCount()) : 1;
    const QSizeF size = m_setup.sizePx();
    const qreal height = pages * pageStride() - kPageGap;
    m_scene->setSceneRect(QRectF(-kSceneMargin, -kSceneMargin, size.width() + 2 * kSceneMargin,
                                 height + 2 * kSceneMargin));
    if (pages != m_pageCount) {
        m_pageCount = pages;
        emit pageCountChanged(pages);
    }
    viewport()->update();
}

void PageView::drawBackground(QPainter *painter, const QRectF &rect)
{
    const ThemePalette &pal = ThemeManager::instance().palette();
    painter->fillRect(rect, pal.canvas);

    const QSizeF size = m_setup.sizePx();
    const qreal stride = pageStride();
    const int first = qMax(0, int(std::floor(rect.top() / stride)));
    const int last = qMin(m_pageCount - 1, int(std::floor(rect.bottom() / stride)));
    const QColor pageColor = m_darkPage ? QColor(0x24, 0x24, 0x24) : QColor(Qt::white);
    for (int i = first; i <= last; ++i) {
        const QRectF page(0, i * stride, size.width(), size.height());
        for (int s = 3; s >= 1; --s)
            painter->fillRect(page.adjusted(-s * 0.6, -s * 0.2, s * 0.6, s * 1.0), QColor(0, 0, 0, pal.dark ? 34 : 12));
        painter->fillRect(page, pageColor);
        drawHeaderFooter(painter, page, i + 1);
    }
}

void PageView::drawHeaderFooter(QPainter *painter, const QRectF &pageRect, int page) const
{
    if (m_setup.header.isEmpty() && m_setup.footer.isEmpty())
        return;
    auto expand = [&](QString text) {
        text.replace(QLatin1String("{page}"), QString::number(page));
        text.replace(QLatin1String("{pages}"), QString::number(m_pageCount));
        text.replace(QLatin1String("{date}"), QLocale().toString(QDate::currentDate(), QLocale::ShortFormat));
        return text;
    };
    const QMarginsF m = m_setup.marginsPx();
    QFont font(QStringLiteral("Calibri"));
    font.setPointSizeF(9);
    painter->save();
    painter->setFont(font);
    painter->setPen(m_darkPage ? QColor(0xA0, 0xA0, 0xA0) : QColor(0x70, 0x70, 0x70));
    const qreal width = pageRect.width() - m.left() - m.right();
    if (!m_setup.header.isEmpty())
        painter->drawText(QRectF(pageRect.left() + m.left(), pageRect.top(), width, m.top()),
                          Qt::AlignHCenter | Qt::AlignVCenter, expand(m_setup.header));
    if (!m_setup.footer.isEmpty())
        painter->drawText(QRectF(pageRect.left() + m.left(), pageRect.bottom() - m.bottom(), width, m.bottom()),
                          Qt::AlignHCenter | Qt::AlignVCenter, expand(m_setup.footer));
    painter->restore();
}

void PageView::setZoomPercent(int percent)
{
    m_fit = FitMode::None;
    applyZoom(percent / 100.0, false);
}

void PageView::applyZoom(qreal zoom, bool underMouse)
{
    zoom = qBound(0.1, zoom, 5.0);
    if (qFuzzyCompare(zoom, m_zoom))
        return;
    setTransformationAnchor(underMouse ? QGraphicsView::AnchorUnderMouse : QGraphicsView::AnchorViewCenter);
    m_zoom = zoom;
    setTransform(QTransform::fromScale(zoom, zoom));
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    emit zoomChanged(zoomPercent());
}

void PageView::setFitMode(FitMode mode)
{
    m_fit = mode;
    applyFit();
}

void PageView::applyFit()
{
    if (m_fit == FitMode::None)
        return;
    const QSizeF size = m_setup.sizePx();
    const qreal byWidth = (viewport()->width() - 2 * kSceneMargin) / size.width();
    qreal zoom = byWidth;
    if (m_fit == FitMode::Page)
        zoom = qMin(byWidth, (viewport()->height() - 2 * kSceneMargin) / size.height());
    applyZoom(zoom, false);
}

void PageView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    applyFit();
}

void PageView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const qreal factor = event->angleDelta().y() > 0 ? 1.1 : 1 / 1.1;
        m_fit = FitMode::None;
        applyZoom(m_zoom * factor, true);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    if (!itemAt(event->pos()) && event->button() == Qt::LeftButton) {
        // Clicks on the canvas around the pages place the cursor at the nearest position.
        const QPointF scenePos = mapToScene(event->pos());
        QPointF local = m_item->mapFromScene(scenePos);
        const QRectF bounds = m_item->boundingRect();
        local.setX(qBound(bounds.left() + 1, local.x(), bounds.right() - 1));
        local.setY(qBound(bounds.top() + 1, local.y(), bounds.bottom() - 1));
        const int pos = document()->documentLayout()->hitTest(local, Qt::FuzzyHit);
        focusText();
        if (pos >= 0) {
            QTextCursor c = textCursor();
            c.setPosition(pos);
            setTextCursor(c);
            emit m_item->cursorMoved();
        }
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

int PageView::currentPage() const
{
    const QRectF r = TextOps::cursorRect(textCursor());
    return qBound(1, int(r.center().y() / pageStride()) + 1, m_pageCount);
}

void PageView::ensureCursorVisible()
{
    const QRectF r = m_item->mapRectToScene(TextOps::cursorRect(textCursor()));
    if (r.isValid() || !r.isNull())
        ensureVisible(r, 30, 60);
}

void PageView::scrollToPosition(int position)
{
    QTextCursor c(document());
    c.setPosition(qBound(0, position, document()->characterCount() - 1));
    const QRectF r = m_item->mapRectToScene(TextOps::cursorRect(c));
    centerOn(QPointF(m_scene->sceneRect().center().x(), r.top() + viewport()->height() / (2.5 * m_zoom)));
}

void PageView::focusText()
{
    setFocus(Qt::OtherFocusReason);
    m_scene->setFocusItem(m_item, Qt::OtherFocusReason);
}

void PageView::setDarkPage(bool dark)
{
    m_darkPage = dark;
    m_item->setDefaultTextColor(dark ? QColor(0xE8, 0xE8, 0xE8) : QColor(Qt::black));
    viewport()->update();
}
