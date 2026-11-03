#pragma once

#include "formats/PageSetup.h"

#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QUrl>

#include <functional>

// Editable text item that reports interactions the editor needs to react to.
class PageTextItem : public QGraphicsTextItem
{
    Q_OBJECT
public:
    explicit PageTextItem(QGraphicsItem *parent = nullptr);

    // Search highlights in document coordinates.
    void setHighlights(const QList<QRectF> &rects, const QList<QRectF> &current);
    // Called before default key handling; return true to consume the event.
    std::function<bool(QKeyEvent *)> keyFilter;

signals:
    void cursorMoved();
    void enterPressed();
    void pasteRequested();
    void contextMenuRequested(const QPoint &screenPos);
    void linkClicked(const QString &href);
    void imageDoubleClicked(int position);
    void filesDropped(const QList<QUrl> &urls, int position);
    void mouseReleased();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QList<QRectF> m_highlights;
    QList<QRectF> m_current;
};

// Print layout view: shows the document as paginated sheets with real zoom.
class PageView : public QGraphicsView
{
    Q_OBJECT
public:
    enum class FitMode { None, Width, Page };

    explicit PageView(QWidget *parent = nullptr);

    void setDocument(QTextDocument *document);
    QTextDocument *document() const;
    PageTextItem *textItem() const { return m_item; }
    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &cursor);

    void setPageSetup(const PageSetup &setup);
    const PageSetup &pageSetup() const { return m_setup; }

    int zoomPercent() const { return qRound(m_zoom * 100); }
    void setZoomPercent(int percent);
    void setFitMode(FitMode mode);
    FitMode fitMode() const { return m_fit; }

    int pageCount() const { return m_pageCount; }
    int currentPage() const;
    void ensureCursorVisible();
    void scrollToPosition(int position);
    void focusText();

    void setDarkPage(bool dark);
    bool isDarkPage() const { return m_darkPage; }

signals:
    void zoomChanged(int percent);
    void pageCountChanged(int count);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void applyZoom(qreal zoom, bool underMouse);
    void applyFit();
    void applyPageSetup();
    void updatePages();
    void drawHeaderFooter(QPainter *painter, const QRectF &pageRect, int page) const;
    qreal pageStride() const;

    QGraphicsScene *m_scene = nullptr;
    PageTextItem *m_item = nullptr;
    PageSetup m_setup;
    qreal m_zoom = 1.0;
    int m_pageCount = 1;
    FitMode m_fit = FitMode::None;
    bool m_darkPage = false;
};
