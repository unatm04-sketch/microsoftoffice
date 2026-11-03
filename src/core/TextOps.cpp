#include "core/TextOps.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QImage>
#include <QLocale>
#include <QPixmap>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QTextList>
#include <QTextTable>
#include <QUrl>
#include <QUuid>

namespace TextOps {

namespace {

template<typename Fn>
void forEachSelectedBlock(const QTextCursor &cursor, Fn fn)
{
    QTextDocument *doc = cursor.document();
    QTextBlock block = doc->findBlock(cursor.selectionStart());
    const QTextBlock last = doc->findBlock(cursor.selectionEnd());
    while (block.isValid()) {
        fn(block);
        if (block == last)
            break;
        block = block.next();
    }
}

void removeFromList(QTextCursor &cursor)
{
    forEachSelectedBlock(cursor, [](const QTextBlock &block) {
        if (QTextList *list = block.textList())
            list->remove(block);
        QTextCursor c(block);
        QTextBlockFormat bf = block.blockFormat();
        bf.setIndent(0);
        bf.setMarker(QTextBlockFormat::MarkerType::NoMarker);
        bf.setBottomMargin(pointsToPixels(8));
        c.setBlockFormat(bf);
    });
}

// List items get tight spacing, like the contextual spacing of word processors.
void tightenListSpacing(QTextCursor &cursor)
{
    QTextBlockFormat spacing;
    spacing.setBottomMargin(pointsToPixels(2));
    cursor.mergeBlockFormat(spacing);
}

QTextListFormat::Style bulletForLevel(int level)
{
    switch ((level - 1) % 3) {
    case 0: return QTextListFormat::ListDisc;
    case 1: return QTextListFormat::ListCircle;
    default: return QTextListFormat::ListSquare;
    }
}

bool isBulletStyle(QTextListFormat::Style style)
{
    return style == QTextListFormat::ListDisc || style == QTextListFormat::ListCircle
           || style == QTextListFormat::ListSquare;
}

QString convertCase(const QString &text, CaseMode mode)
{
    const QLocale tr(QLocale::Turkish, QLocale::Turkey);
    switch (mode) {
    case CaseMode::Lower:
        return tr.toLower(text);
    case CaseMode::Upper:
        return tr.toUpper(text);
    case CaseMode::Toggle: {
        QString out;
        out.reserve(text.size());
        for (const QChar ch : text) {
            const QString s(ch);
            out += ch.isUpper() ? tr.toLower(s) : tr.toUpper(s);
        }
        return out;
    }
    case CaseMode::Capitalize: {
        QString out = tr.toLower(text);
        bool startOfWord = true;
        for (int i = 0; i < out.size(); ++i) {
            if (out.at(i).isLetter()) {
                if (startOfWord) {
                    const QString up = tr.toUpper(QString(out.at(i)));
                    if (up.size() == 1)
                        out[i] = up.at(0);
                }
                startOfWord = false;
            } else {
                startOfWord = !(out.at(i) == QLatin1Char('\'') || out.at(i) == QChar(0x2019));
            }
        }
        return out;
    }
    case CaseMode::Sentence: {
        QString out = tr.toLower(text);
        bool startOfSentence = true;
        for (int i = 0; i < out.size(); ++i) {
            const QChar ch = out.at(i);
            if (ch.isLetter()) {
                if (startOfSentence) {
                    const QString up = tr.toUpper(QString(ch));
                    if (up.size() == 1)
                        out[i] = up.at(0);
                }
                startOfSentence = false;
            } else if (ch == QLatin1Char('.') || ch == QLatin1Char('!') || ch == QLatin1Char('?')
                       || ch == QChar::ParagraphSeparator) {
                startOfSentence = true;
            }
        }
        return out;
    }
    }
    return text;
}

} // namespace

const QList<ParagraphStyle> &paragraphStyles()
{
    static const QList<ParagraphStyle> styles = [] {
        auto tr = [](const char *text) { return QCoreApplication::translate("TextOps", text); };
        QList<ParagraphStyle> list;
        ParagraphStyle normal;
        normal.id = QStringLiteral("Normal");
        normal.name = tr("Normal");
        list << normal;

        ParagraphStyle h1;
        h1.id = QStringLiteral("Heading1");
        h1.name = tr("Başlık 1");
        h1.headingLevel = 1;
        h1.pointSize = 16;
        h1.color = QColor(0x2F, 0x54, 0x96);
        h1.family = QStringLiteral("Calibri Light");
        h1.spaceBefore = 12;
        h1.spaceAfter = 4;
        list << h1;

        ParagraphStyle h2 = h1;
        h2.id = QStringLiteral("Heading2");
        h2.name = tr("Başlık 2");
        h2.headingLevel = 2;
        h2.pointSize = 13;
        h2.spaceBefore = 8;
        list << h2;

        ParagraphStyle h3 = h1;
        h3.id = QStringLiteral("Heading3");
        h3.name = tr("Başlık 3");
        h3.headingLevel = 3;
        h3.pointSize = 12;
        h3.color = QColor(0x1F, 0x37, 0x63);
        h3.spaceBefore = 6;
        list << h3;

        ParagraphStyle title;
        title.id = QStringLiteral("Title");
        title.name = tr("Konu Başlığı");
        title.pointSize = 28;
        title.family = QStringLiteral("Calibri Light");
        title.spaceAfter = 4;
        list << title;

        ParagraphStyle subtitle;
        subtitle.id = QStringLiteral("Subtitle");
        subtitle.name = tr("Alt Konu Başlığı");
        subtitle.pointSize = 12;
        subtitle.color = QColor(0x5A, 0x5A, 0x5A);
        list << subtitle;

        ParagraphStyle quote;
        quote.id = QStringLiteral("Quote");
        quote.name = tr("Alıntı");
        quote.italic = true;
        quote.color = QColor(0x40, 0x40, 0x40);
        quote.spaceBefore = 10;
        quote.leftIndent = 58;
        quote.alignment = Qt::AlignHCenter;
        list << quote;

        ParagraphStyle code;
        code.id = QStringLiteral("Code");
        code.name = tr("Kod");
        code.pointSize = 10;
        code.family = QStringLiteral("Consolas");
        code.spaceAfter = 0;
        code.background = QColor(0xF2, 0xF2, 0xF2);
        list << code;
        return list;
    }();
    return styles;
}

const ParagraphStyle *paragraphStyle(const QString &id)
{
    for (const ParagraphStyle &s : paragraphStyles()) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

QString styleIdOf(const QTextBlock &block)
{
    const QTextBlockFormat bf = block.blockFormat();
    const QString id = bf.stringProperty(StyleIdProperty);
    if (!id.isEmpty())
        return id;
    if (bf.headingLevel() > 0 && bf.headingLevel() <= 3)
        return QStringLiteral("Heading%1").arg(bf.headingLevel());
    return QStringLiteral("Normal");
}

QFont defaultFont()
{
    QFont font(QStringLiteral("Calibri"));
    font.setPointSizeF(11);
    return font;
}

qreal pointsToPixels(qreal points)
{
    return points * 96.0 / 72.0;
}

void setupDocument(QTextDocument *document)
{
    document->setDefaultFont(defaultFont());
    document->setIndentWidth(48);
    QTextCursor cursor(document);
    QTextBlockFormat bf;
    bf.setBottomMargin(pointsToPixels(8));
    bf.setLineHeight(108, QTextBlockFormat::ProportionalHeight);
    cursor.setBlockFormat(bf);
    document->clearUndoRedoStacks();
    document->setModified(false);
}

void mergeCharFormat(QTextCursor &cursor, const QTextCharFormat &format)
{
    if (!cursor.hasSelection()) {
        QTextCursor word = cursor;
        word.select(QTextCursor::WordUnderCursor);
        if (word.hasSelection() && word.selectionStart() < cursor.position()
            && cursor.position() < word.selectionEnd()) {
            word.mergeCharFormat(format);
        }
    }
    cursor.mergeCharFormat(format);
}

void clearCharProperty(QTextCursor &cursor, int property)
{
    QTextDocument *doc = cursor.document();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    if (!cursor.hasSelection()) {
        QTextCursor word = cursor;
        word.select(QTextCursor::WordUnderCursor);
        if (word.hasSelection() && word.selectionStart() < cursor.position() && cursor.position() < word.selectionEnd()) {
            start = word.selectionStart();
            end = word.selectionEnd();
        }
    }
    cursor.beginEditBlock();
    QTextBlock block = doc->findBlock(start);
    while (block.isValid() && block.position() < end) {
        QList<QPair<int, int>> ranges;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            const int s = qMax(start, frag.position());
            const int e = qMin(end, frag.position() + frag.length());
            if (frag.isValid() && s < e)
                ranges << qMakePair(s, e);
        }
        for (const auto &r : std::as_const(ranges)) {
            QTextCursor fc(doc);
            fc.setPosition(r.first);
            fc.setPosition(r.second, QTextCursor::KeepAnchor);
            QTextCharFormat f = fc.charFormat();
            f.clearProperty(property);
            fc.setCharFormat(f);
        }
        block = block.next();
    }
    QTextCharFormat insertion = cursor.charFormat();
    insertion.clearProperty(property);
    if (!cursor.hasSelection())
        cursor.setCharFormat(insertion);
    cursor.endEditBlock();
}

void clearFormatting(QTextCursor &cursor)
{
    cursor.beginEditBlock();
    cursor.setCharFormat(QTextCharFormat());
    applyParagraphStyle(cursor, QStringLiteral("Normal"));
    cursor.endEditBlock();
}

void setAlignment(QTextCursor &cursor, Qt::Alignment alignment)
{
    QTextBlockFormat bf;
    bf.setAlignment(alignment);
    cursor.mergeBlockFormat(bf);
}

void setLineSpacing(QTextCursor &cursor, int percent)
{
    QTextBlockFormat bf;
    bf.setLineHeight(percent, QTextBlockFormat::ProportionalHeight);
    cursor.mergeBlockFormat(bf);
}

void setParagraphSpacing(QTextCursor &cursor, qreal beforePt, qreal afterPt)
{
    QTextBlockFormat bf;
    if (beforePt >= 0)
        bf.setTopMargin(pointsToPixels(beforePt));
    if (afterPt >= 0)
        bf.setBottomMargin(pointsToPixels(afterPt));
    cursor.mergeBlockFormat(bf);
}

void changeIndent(QTextCursor &cursor, int delta)
{
    cursor.beginEditBlock();
    if (QTextList *list = cursor.currentList()) {
        QTextListFormat lf = list->format();
        const int newIndent = lf.indent() + delta;
        if (newIndent < 1) {
            removeFromList(cursor);
        } else {
            lf.setIndent(newIndent);
            if (isBulletStyle(lf.style()))
                lf.setStyle(bulletForLevel(newIndent));
            cursor.createList(lf);
        }
    } else {
        forEachSelectedBlock(cursor, [delta](const QTextBlock &block) {
            QTextCursor c(block);
            QTextBlockFormat bf = block.blockFormat();
            bf.setIndent(qMax(0, bf.indent() + delta));
            c.setBlockFormat(bf);
        });
    }
    cursor.endEditBlock();
}

void toggleList(QTextCursor &cursor, QTextListFormat::Style style)
{
    cursor.beginEditBlock();
    QTextList *list = cursor.currentList();
    const bool checklist = cursor.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker;
    if (list && list->format().style() == style && !checklist) {
        removeFromList(cursor);
    } else if (list) {
        QTextListFormat lf = list->format();
        lf.setStyle(style);
        list->setFormat(lf);
        forEachSelectedBlock(cursor, [](const QTextBlock &block) {
            QTextCursor c(block);
            QTextBlockFormat bf = block.blockFormat();
            bf.setMarker(QTextBlockFormat::MarkerType::NoMarker);
            c.setBlockFormat(bf);
        });
    } else {
        QTextListFormat lf;
        lf.setStyle(style);
        lf.setIndent(qMax(1, cursor.blockFormat().indent() + 1));
        QTextBlockFormat reset;
        reset.setIndent(0);
        cursor.mergeBlockFormat(reset);
        cursor.createList(lf);
    }
    cursor.endEditBlock();
}

void toggleChecklist(QTextCursor &cursor)
{
    cursor.beginEditBlock();
    const bool checklist = cursor.currentList()
                           && cursor.blockFormat().marker() != QTextBlockFormat::MarkerType::NoMarker;
    if (checklist) {
        removeFromList(cursor);
    } else {
        if (!cursor.currentList()) {
            QTextListFormat lf;
            lf.setStyle(QTextListFormat::ListDisc);
            lf.setIndent(1);
            cursor.createList(lf);
        }
        QTextBlockFormat bf;
        bf.setMarker(QTextBlockFormat::MarkerType::Unchecked);
        cursor.mergeBlockFormat(bf);
    }
    cursor.endEditBlock();
}

void applyParagraphStyle(QTextCursor &cursor, const QString &styleId)
{
    const ParagraphStyle *style = paragraphStyle(styleId);
    if (!style)
        return;
    QTextDocument *doc = cursor.document();

    auto styledCharFormat = [style](QTextCharFormat f) {
        f.setFontPointSize(style->pointSize);
        f.setFontWeight(style->bold ? QFont::Bold : QFont::Normal);
        f.setFontItalic(style->italic);
        if (style->color.isValid())
            f.setForeground(style->color);
        else
            f.clearForeground();
        if (!style->family.isEmpty())
            f.setFontFamilies(QStringList{style->family});
        else
            f.clearProperty(QTextFormat::FontFamilies);
        return f;
    };

    cursor.beginEditBlock();
    forEachSelectedBlock(cursor, [&](const QTextBlock &block) {
        QTextCursor bc(block);
        QTextBlockFormat bf = block.blockFormat();
        bf.setHeadingLevel(style->headingLevel);
        bf.setProperty(StyleIdProperty, style->id);
        bf.setTopMargin(pointsToPixels(style->spaceBefore));
        bf.setBottomMargin(pointsToPixels(style->spaceAfter));
        bf.setLeftMargin(style->leftIndent);
        bf.setRightMargin(style->leftIndent);
        bf.setAlignment(style->alignment);
        if (style->background.isValid())
            bf.setBackground(style->background);
        else
            bf.clearBackground();
        bc.setBlockFormat(bf);

        // Restyle every fragment while keeping run-level properties such as links.
        QList<QPair<int, int>> ranges;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (frag.isValid() && !frag.charFormat().isImageFormat())
                ranges << qMakePair(frag.position(), frag.length());
        }
        for (const auto &r : std::as_const(ranges)) {
            QTextCursor fc(doc);
            fc.setPosition(r.first);
            fc.setPosition(r.first + r.second, QTextCursor::KeepAnchor);
            fc.setCharFormat(styledCharFormat(fc.charFormat()));
        }
        bc.setBlockCharFormat(styledCharFormat(block.charFormat()));
    });
    cursor.setCharFormat(styledCharFormat(cursor.charFormat()));
    cursor.endEditBlock();
}

void insertTable(QTextCursor &cursor, int rows, int columns)
{
    QTextTableFormat tf;
    tf.setBorder(1);
    tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    tf.setBorderBrush(QColor(0x8C, 0x8C, 0x8C));
    tf.setBorderCollapse(true);
    tf.setCellPadding(5);
    tf.setCellSpacing(0);
    tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
    QList<QTextLength> widths;
    for (int i = 0; i < columns; ++i)
        widths << QTextLength(QTextLength::PercentageLength, 100.0 / columns);
    tf.setColumnWidthConstraints(widths);

    cursor.beginEditBlock();
    QTextTable *table = cursor.insertTable(rows, columns, tf);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            QTextCursor cc = table->cellAt(r, c).firstCursorPosition();
            QTextBlockFormat bf = cc.blockFormat();
            bf.setTopMargin(0);
            bf.setBottomMargin(0);
            bf.setLineHeight(100, QTextBlockFormat::ProportionalHeight);
            bf.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
            cc.setBlockFormat(bf);
        }
    }
    cursor.endEditBlock();
    cursor = table->cellAt(0, 0).firstCursorPosition();
}

void insertHyperlink(QTextCursor &cursor, const QString &text, const QString &url)
{
    QTextCharFormat base = cursor.charFormat();
    base.setAnchor(false);
    base.clearProperty(QTextFormat::AnchorHref);

    QTextCharFormat link = base;
    link.setAnchor(true);
    link.setAnchorHref(url);
    link.setForeground(QColor(0x05, 0x63, 0xC1));
    link.setFontUnderline(true);

    cursor.beginEditBlock();
    if (cursor.hasSelection() && (text.isEmpty() || text == cursor.selectedText())) {
        cursor.mergeCharFormat(link);
        cursor.setPosition(cursor.selectionEnd());
    } else {
        cursor.insertText(text.isEmpty() ? url : text, link);
    }
    cursor.setCharFormat(base);
    cursor.endEditBlock();
}

void removeHyperlink(QTextCursor &cursor)
{
    QTextCursor c = cursor;
    if (!c.hasSelection()) {
        const QString href = c.charFormat().anchorHref();
        if (href.isEmpty())
            return;
        int start = c.position();
        int end = c.position();
        QTextCursor probe(c.document());
        while (start > c.block().position()) {
            probe.setPosition(start);
            if (probe.charFormat().anchorHref() != href)
                break;
            --start;
        }
        if (probe.charFormat().anchorHref() != href)
            ++start;
        const int blockEnd = c.block().position() + c.block().length() - 1;
        while (end < blockEnd) {
            probe.setPosition(end + 1);
            if (probe.charFormat().anchorHref() != href)
                break;
            ++end;
        }
        c.setPosition(qMax(start, c.block().position()));
        c.setPosition(end, QTextCursor::KeepAnchor);
    }
    QTextCharFormat plain;
    plain.setAnchor(false);
    plain.setAnchorHref(QString());
    plain.setFontUnderline(false);
    c.mergeCharFormat(plain);
    QTextCharFormat noColor = c.charFormat();
    noColor.clearForeground();
    noColor.clearProperty(QTextFormat::AnchorHref);
    noColor.setAnchor(false);
    c.setCharFormat(noColor);
}

void insertHorizontalRule(QTextCursor &cursor)
{
    cursor.beginEditBlock();
    QTextBlockFormat normal = cursor.blockFormat();
    normal.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
    normal.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
    if (cursor.block().length() > 1)
        cursor.insertBlock(normal);
    QTextBlockFormat hr = normal;
    hr.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth, QTextLength(QTextLength::PercentageLength, 100));
    cursor.setBlockFormat(hr);
    cursor.insertBlock(normal);
    cursor.endEditBlock();
}

void insertPageBreak(QTextCursor &cursor)
{
    cursor.beginEditBlock();
    QTextBlockFormat bf = cursor.blockFormat();
    bf.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
    bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
    cursor.insertBlock(bf);
    cursor.endEditBlock();
}

QString insertImage(QTextCursor &cursor, const QImage &source, qreal maxWidth)
{
    QImage image = source;
    if (image.width() > 2400)
        image = image.scaledToWidth(2400, Qt::SmoothTransformation);
    QTextDocument *doc = cursor.document();
    const QString name = QStringLiteral("omni-image-%1.png").arg(QUuid::createUuid().toString(QUuid::Id128));
    doc->addResource(QTextDocument::ImageResource, QUrl(name), image);
    QTextImageFormat f;
    f.setName(name);
    qreal w = source.width();
    qreal h = source.height();
    if (maxWidth > 0 && w > maxWidth) {
        h = h * maxWidth / w;
        w = maxWidth;
    }
    f.setWidth(w);
    f.setHeight(h);
    cursor.insertImage(f);
    return name;
}

void embedImageResources(QTextDocument *document)
{
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat())
                continue;
            const QUrl url(frag.charFormat().toImageFormat().name());
            const QVariant res = document->resource(QTextDocument::ImageResource, url);
            QImage img;
            if (res.canConvert<QImage>())
                img = res.value<QImage>();
            else if (res.canConvert<QPixmap>())
                img = res.value<QPixmap>().toImage();
            else if (res.typeId() == QMetaType::QByteArray)
                img.loadFromData(res.toByteArray());
            if (!img.isNull())
                document->addResource(QTextDocument::ImageResource, url, img);
        }
    }
}

void changeCase(QTextCursor &cursor, CaseMode mode)
{
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    if (!cursor.hasSelection())
        return;
    QTextDocument *doc = cursor.document();
    const int start = cursor.selectionStart();
    const int end = cursor.selectionEnd();
    const QString original = cursor.selectedText();
    const QString converted = convertCase(original, mode);

    cursor.beginEditBlock();
    if (converted.size() != original.size()) {
        const QTextCharFormat fmt = cursor.charFormat();
        cursor.insertText(QString(converted).replace(QChar::ParagraphSeparator, QLatin1Char('\n')), fmt);
    } else {
        // Replace fragment by fragment so that character formatting is preserved.
        int pos = start;
        while (pos < end) {
            const QTextBlock block = doc->findBlock(pos);
            QTextFragment frag;
            for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
                if (it.fragment().contains(pos)) {
                    frag = it.fragment();
                    break;
                }
            }
            if (!frag.isValid()) {
                pos = block.position() + block.length();
                continue;
            }
            const int segEnd = qMin(end, frag.position() + frag.length());
            if (!frag.charFormat().isImageFormat()) {
                QTextCursor c(doc);
                c.setPosition(pos);
                c.setPosition(segEnd, QTextCursor::KeepAnchor);
                c.insertText(converted.mid(pos - start, segEnd - pos), frag.charFormat());
            }
            pos = segEnd;
        }
    }
    cursor.endEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(start + converted.size(), QTextCursor::KeepAnchor);
}

int wordCount(const QString &text)
{
    int count = 0;
    bool inWord = false;
    for (const QChar ch : text) {
        const bool wordChar = ch.isLetterOrNumber()
                              || (inWord && (ch == QLatin1Char('\'') || ch == QChar(0x2019) || ch == QLatin1Char('-')));
        if (wordChar && !inWord)
            ++count;
        inWord = wordChar;
    }
    return count;
}

QList<QRectF> rangeRects(const QTextDocument *document, int start, int end)
{
    QList<QRectF> rects;
    QAbstractTextDocumentLayout *layout = document->documentLayout();
    QTextBlock block = document->findBlock(start);
    while (block.isValid() && block.position() < end) {
        const QTextLayout *tl = block.layout();
        const QRectF br = layout->blockBoundingRect(block);
        const int bStart = qMax(start, block.position()) - block.position();
        const int bEnd = qMin(end, block.position() + block.length() - 1) - block.position();
        for (int i = 0; tl && i < tl->lineCount(); ++i) {
            const QTextLine line = tl->lineAt(i);
            const int ls = line.textStart();
            const int le = ls + line.textLength();
            const int s = qMax(bStart, ls);
            const int e = qMin(bEnd, le);
            if (s >= e)
                continue;
            const qreal x1 = line.cursorToX(s);
            const qreal x2 = line.cursorToX(e);
            rects << QRectF(br.left() + qMin(x1, x2), br.top() + line.y(), qAbs(x2 - x1), line.height());
        }
        block = block.next();
    }
    return rects;
}

QRectF cursorRect(const QTextCursor &cursor)
{
    if (cursor.isNull())
        return {};
    const QTextBlock block = cursor.block();
    const QTextLayout *tl = block.layout();
    if (!tl)
        return {};
    const QRectF br = cursor.document()->documentLayout()->blockBoundingRect(block);
    const int pos = cursor.positionInBlock();
    QTextLine line = tl->lineForTextPosition(pos);
    if (!line.isValid()) {
        if (tl->lineCount() == 0)
            return QRectF(br.topLeft(), QSizeF(1, br.height()));
        line = tl->lineAt(tl->lineCount() - 1);
    }
    const qreal x = line.cursorToX(pos);
    return QRectF(br.left() + x, br.top() + line.y(), 1.5, line.height());
}

QList<QTextCursor> findAll(QTextDocument *document, const QString &text, bool caseSensitive,
                           bool wholeWords, bool regex)
{
    QList<QTextCursor> results;
    if (text.isEmpty())
        return results;
    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    if (wholeWords)
        flags |= QTextDocument::FindWholeWords;
    QRegularExpression re;
    if (regex) {
        re = QRegularExpression(text, caseSensitive ? QRegularExpression::NoPatternOption
                                                    : QRegularExpression::CaseInsensitiveOption);
        if (!re.isValid())
            return results;
    }
    QTextCursor from(document);
    while (results.size() < 10000) {
        const QTextCursor found = regex ? document->find(re, from, flags) : document->find(text, from, flags);
        if (found.isNull())
            break;
        if (found.selectionStart() == found.selectionEnd()) {
            from.setPosition(found.position() + 1);
            if (from.position() >= document->characterCount() - 1)
                break;
            continue;
        }
        results << found;
        from = found;
    }
    return results;
}

void afterNewParagraph(QTextCursor &cursor)
{
    const QTextBlock current = cursor.block();
    const QTextBlock previous = current.previous();
    if (!previous.isValid())
        return;
    QTextBlockFormat bf = current.blockFormat();
    bool changed = false;
    if ((bf.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore)
        && (previous.blockFormat().pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore)) {
        bf.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
        changed = true;
    }
    if (bf.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
        bf.clearProperty(QTextFormat::BlockTrailingHorizontalRulerWidth);
        changed = true;
    }
    if (bf.marker() == QTextBlockFormat::MarkerType::Checked) {
        bf.setMarker(QTextBlockFormat::MarkerType::Unchecked);
        changed = true;
    }
    if (changed)
        cursor.setBlockFormat(bf);

    // Like word processors, pressing Enter at the end of a heading continues with Normal text.
    const QString style = styleIdOf(current);
    if (current.length() <= 1 && style != QStringLiteral("Normal") && style != QStringLiteral("Code"))
        applyParagraphStyle(cursor, QStringLiteral("Normal"));
}

} // namespace TextOps
