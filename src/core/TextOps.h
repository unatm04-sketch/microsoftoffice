#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QRectF>
#include <QString>
#include <QTextCursor>
#include <QTextFormat>
#include <QTextListFormat>

class QImage;
class QTextBlock;
class QTextDocument;

// Rich text editing helpers shared by the document, notes and mail editors.
namespace TextOps {

// Block property holding the paragraph style id ("Normal", "Heading1", ...).
inline constexpr int StyleIdProperty = QTextFormat::UserProperty + 1;

struct ParagraphStyle
{
    QString id;
    QString name;
    int headingLevel = 0;
    qreal pointSize = 11;
    bool bold = false;
    bool italic = false;
    QColor color;          // invalid: automatic
    QString family;        // empty: document default
    qreal spaceBefore = 0; // points
    qreal spaceAfter = 8;  // points
    qreal leftIndent = 0;  // pixels
    QColor background;
    Qt::Alignment alignment = Qt::AlignLeft;
};

const QList<ParagraphStyle> &paragraphStyles();
const ParagraphStyle *paragraphStyle(const QString &id);
QString styleIdOf(const QTextBlock &block);

QFont defaultFont();
// Default font, indentation and first paragraph spacing for new documents.
void setupDocument(QTextDocument *document);
qreal pointsToPixels(qreal points);

// Character formatting: applies to the selection, the word under the cursor,
// or the insertion format when the cursor is between words.
void mergeCharFormat(QTextCursor &cursor, const QTextCharFormat &format);
// Removes a character property (e.g. ForegroundBrush) from the selection or insertion format.
void clearCharProperty(QTextCursor &cursor, int property);
void clearFormatting(QTextCursor &cursor);

void setAlignment(QTextCursor &cursor, Qt::Alignment alignment);
void setLineSpacing(QTextCursor &cursor, int percent);
void setParagraphSpacing(QTextCursor &cursor, qreal beforePt, qreal afterPt);
void changeIndent(QTextCursor &cursor, int delta);
void toggleList(QTextCursor &cursor, QTextListFormat::Style style);
void toggleChecklist(QTextCursor &cursor);
void applyParagraphStyle(QTextCursor &cursor, const QString &styleId);
// Fixes up the paragraph created by Enter (page breaks, rules, heading -> Normal).
void afterNewParagraph(QTextCursor &cursor);

void insertTable(QTextCursor &cursor, int rows, int columns);
void insertHyperlink(QTextCursor &cursor, const QString &text, const QString &url);
void removeHyperlink(QTextCursor &cursor);
void insertHorizontalRule(QTextCursor &cursor);
void insertPageBreak(QTextCursor &cursor);
// Adds `image` as a document resource and inserts it; returns the resource name.
QString insertImage(QTextCursor &cursor, const QImage &image, qreal maxWidth);
// Makes images referenced by the document permanent resources (needed before cloning).
void embedImageResources(QTextDocument *document);

enum class CaseMode { Sentence, Lower, Upper, Capitalize, Toggle };
void changeCase(QTextCursor &cursor, CaseMode mode);

int wordCount(const QString &text);
// Rectangles (document coordinates) covering the range [start, end).
QList<QRectF> rangeRects(const QTextDocument *document, int start, int end);
QRectF cursorRect(const QTextCursor &cursor);
QList<QTextCursor> findAll(QTextDocument *document, const QString &text, bool caseSensitive,
                           bool wholeWords, bool regex);

} // namespace TextOps
