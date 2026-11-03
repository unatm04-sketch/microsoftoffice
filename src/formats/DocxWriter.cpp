#include "formats/Docx.h"

#include "formats/Ooxml.h"
#include "formats/Zip.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>
#include <QUrl>
#include <QXmlStreamWriter>

namespace {

const QString kW = QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main");
const QString kR = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const QString kWp = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing");
const QString kA = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main");
const QString kPic = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/picture");

QString rel(const char *type)
{
    return QString::fromLatin1(Ooxml::kRelBase) + QLatin1String(type);
}

int twips(qreal px) { return qRound(px * 15.0); }
int mmToTwips(qreal mm) { return qRound(mm / 25.4 * 1440.0); }
qint64 emu(qreal px) { return qRound64(px * 9525.0); }
QString hex(const QColor &c) { return c.name().mid(1).toUpper(); }
QString num(qint64 v) { return QString::number(v); }

QString highlightName(const QColor &c)
{
    static const QHash<QRgb, QString> names = {
        {qRgb(0xFF, 0xFF, 0x00), QStringLiteral("yellow")},   {qRgb(0x00, 0xFF, 0x00), QStringLiteral("green")},
        {qRgb(0x00, 0xFF, 0xFF), QStringLiteral("cyan")},     {qRgb(0xFF, 0x00, 0xFF), QStringLiteral("magenta")},
        {qRgb(0x00, 0x00, 0xFF), QStringLiteral("blue")},     {qRgb(0xFF, 0x00, 0x00), QStringLiteral("red")},
        {qRgb(0x00, 0x00, 0x80), QStringLiteral("darkBlue")}, {qRgb(0x00, 0x80, 0x80), QStringLiteral("darkCyan")},
        {qRgb(0x00, 0x80, 0x00), QStringLiteral("darkGreen")}, {qRgb(0x80, 0x00, 0x80), QStringLiteral("darkMagenta")},
        {qRgb(0x80, 0x00, 0x00), QStringLiteral("darkRed")},  {qRgb(0x80, 0x80, 0x00), QStringLiteral("darkYellow")},
        {qRgb(0x80, 0x80, 0x80), QStringLiteral("darkGray")}, {qRgb(0xC0, 0xC0, 0xC0), QStringLiteral("lightGray")},
        {qRgb(0x00, 0x00, 0x00), QStringLiteral("black")}};
    return names.value(c.rgb() | 0xFF000000u);
}

void val(QXmlStreamWriter &w, const QString &element, const QString &value)
{
    w.writeEmptyElement(element);
    w.writeAttribute(QStringLiteral("w:val"), value);
}

const char kStylesXml[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Calibri" w:eastAsia="Calibri" w:hAnsi="Calibri" w:cs="Calibri"/><w:sz w:val="22"/><w:szCs w:val="22"/><w:lang w:val="tr-TR" w:eastAsia="en-US" w:bidi="ar-SA"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after="160" w:line="259" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults>
<w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:qFormat/></w:style>
<w:style w:type="character" w:default="1" w:styleId="DefaultParagraphFont"><w:name w:val="Default Paragraph Font"/><w:uiPriority w:val="1"/><w:semiHidden/><w:unhideWhenUsed/></w:style>
<w:style w:type="table" w:default="1" w:styleId="TableNormal"><w:name w:val="Normal Table"/><w:uiPriority w:val="99"/><w:semiHidden/><w:unhideWhenUsed/><w:tblPr><w:tblInd w:w="0" w:type="dxa"/><w:tblCellMar><w:top w:w="0" w:type="dxa"/><w:left w:w="108" w:type="dxa"/><w:bottom w:w="0" w:type="dxa"/><w:right w:w="108" w:type="dxa"/></w:tblCellMar></w:tblPr></w:style>
<w:style w:type="numbering" w:default="1" w:styleId="NoList"><w:name w:val="No List"/><w:uiPriority w:val="99"/><w:semiHidden/><w:unhideWhenUsed/></w:style>
<w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:uiPriority w:val="9"/><w:qFormat/><w:pPr><w:keepNext/><w:keepLines/><w:spacing w:before="240" w:after="80"/><w:outlineLvl w:val="0"/></w:pPr><w:rPr><w:rFonts w:ascii="Calibri Light" w:hAnsi="Calibri Light"/><w:color w:val="2F5496"/><w:sz w:val="32"/><w:szCs w:val="32"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="heading 2"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:uiPriority w:val="9"/><w:unhideWhenUsed/><w:qFormat/><w:pPr><w:keepNext/><w:keepLines/><w:spacing w:before="160" w:after="80"/><w:outlineLvl w:val="1"/></w:pPr><w:rPr><w:rFonts w:ascii="Calibri Light" w:hAnsi="Calibri Light"/><w:color w:val="2F5496"/><w:sz w:val="26"/><w:szCs w:val="26"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Heading3"><w:name w:val="heading 3"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:uiPriority w:val="9"/><w:unhideWhenUsed/><w:qFormat/><w:pPr><w:keepNext/><w:keepLines/><w:spacing w:before="120" w:after="80"/><w:outlineLvl w:val="2"/></w:pPr><w:rPr><w:rFonts w:ascii="Calibri Light" w:hAnsi="Calibri Light"/><w:color w:val="1F3763"/><w:sz w:val="24"/><w:szCs w:val="24"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Title"><w:name w:val="Title"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:uiPriority w:val="10"/><w:qFormat/><w:pPr><w:spacing w:after="80" w:line="240" w:lineRule="auto"/><w:contextualSpacing/></w:pPr><w:rPr><w:rFonts w:ascii="Calibri Light" w:hAnsi="Calibri Light"/><w:kern w:val="28"/><w:sz w:val="56"/><w:szCs w:val="56"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Subtitle"><w:name w:val="Subtitle"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:uiPriority w:val="11"/><w:qFormat/><w:rPr><w:color w:val="5A5A5A"/><w:sz w:val="24"/><w:szCs w:val="24"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="Quote"><w:name w:val="Quote"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:uiPriority w:val="29"/><w:qFormat/><w:pPr><w:spacing w:before="200"/><w:ind w:left="864" w:right="864"/><w:jc w:val="center"/></w:pPr><w:rPr><w:i/><w:color w:val="404040"/></w:rPr></w:style>
<w:style w:type="paragraph" w:customStyle="1" w:styleId="Code"><w:name w:val="Code"/><w:basedOn w:val="Normal"/><w:qFormat/><w:pPr><w:shd w:val="clear" w:color="auto" w:fill="F2F2F2"/><w:spacing w:after="0" w:line="240" w:lineRule="auto"/></w:pPr><w:rPr><w:rFonts w:ascii="Consolas" w:hAnsi="Consolas"/><w:sz w:val="20"/><w:szCs w:val="20"/></w:rPr></w:style>
<w:style w:type="paragraph" w:styleId="ListParagraph"><w:name w:val="List Paragraph"/><w:basedOn w:val="Normal"/><w:uiPriority w:val="34"/><w:qFormat/><w:pPr><w:ind w:left="720"/><w:contextualSpacing/></w:pPr></w:style>
<w:style w:type="character" w:styleId="Hyperlink"><w:name w:val="Hyperlink"/><w:basedOn w:val="DefaultParagraphFont"/><w:uiPriority w:val="99"/><w:unhideWhenUsed/><w:rPr><w:color w:val="0563C1"/><w:u w:val="single"/></w:rPr></w:style>
<w:style w:type="table" w:styleId="TableGrid"><w:name w:val="Table Grid"/><w:basedOn w:val="TableNormal"/><w:uiPriority w:val="39"/><w:pPr><w:spacing w:after="0" w:line="240" w:lineRule="auto"/></w:pPr><w:tblPr><w:tblBorders><w:top w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:left w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:bottom w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:right w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:insideH w:val="single" w:sz="4" w:space="0" w:color="auto"/><w:insideV w:val="single" w:sz="4" w:space="0" w:color="auto"/></w:tblBorders></w:tblPr></w:style>
</w:styles>)";

const char kSettingsXml[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:settings xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:zoom w:percent="100"/><w:defaultTabStop w:val="708"/><w:characterSpacingControl w:val="doNotCompress"/><w:compat><w:compatSetting w:name="compatibilityMode" w:uri="http://schemas.microsoft.com/office/word" w:val="15"/></w:compat></w:settings>)";

struct ListInfo
{
    int numId = 0;
    int level = 0;
    QTextListFormat format;
};

class DocxBuilder
{
public:
    DocxBuilder(const QTextDocument *doc, const PageSetup &page, const DocxMetadata &meta)
        : m_doc(doc), m_page(page), m_meta(meta)
    {
        const QSizeF mm = page.sizeMm();
        m_textWidthTwips = mmToTwips(mm.width() - page.margins.left() - page.margins.right());
    }

    QByteArray build()
    {
        m_rels.add(rel("styles"), QStringLiteral("styles.xml"));
        m_rels.add(rel("settings"), QStringLiteral("settings.xml"));
        m_rels.add(rel("numbering"), QStringLiteral("numbering.xml"));
        if (!m_page.header.isEmpty())
            m_headerRel = m_rels.add(rel("header"), QStringLiteral("header1.xml"));
        if (!m_page.footer.isEmpty())
            m_footerRel = m_rels.add(rel("footer"), QStringLiteral("footer1.xml"));

        const QByteArray documentXml = buildDocument();

        ZipWriter zip;
        zip.add(QStringLiteral("[Content_Types].xml"), contentTypes());
        zip.add(QStringLiteral("_rels/.rels"), rootRels());
        zip.add(QStringLiteral("docProps/core.xml"), Ooxml::corePropertiesXml(m_meta.title, m_meta.author));
        zip.add(QStringLiteral("docProps/app.xml"), Ooxml::appPropertiesXml());
        zip.add(QStringLiteral("word/document.xml"), documentXml);
        zip.add(QStringLiteral("word/styles.xml"), QByteArray(kStylesXml));
        zip.add(QStringLiteral("word/settings.xml"), QByteArray(kSettingsXml));
        zip.add(QStringLiteral("word/numbering.xml"), numberingXml());
        if (!m_headerRel.isEmpty())
            zip.add(QStringLiteral("word/header1.xml"), headerFooterXml(true, m_page.header));
        if (!m_footerRel.isEmpty())
            zip.add(QStringLiteral("word/footer1.xml"), headerFooterXml(false, m_page.footer));
        zip.add(QStringLiteral("word/_rels/document.xml.rels"), m_rels.toXml());
        for (const auto &media : std::as_const(m_media))
            zip.add(QStringLiteral("word/media/") + media.first, media.second, false);
        return zip.finish();
    }

private:
    QByteArray buildDocument()
    {
        QByteArray out;
        QXmlStreamWriter w(&out);
        w.writeStartDocument(QStringLiteral("1.0"), true);
        w.writeStartElement(QStringLiteral("w:document"));
        w.writeAttribute(QStringLiteral("xmlns:w"), kW);
        w.writeAttribute(QStringLiteral("xmlns:r"), kR);
        w.writeAttribute(QStringLiteral("xmlns:wp"), kWp);
        w.writeAttribute(QStringLiteral("xmlns:a"), kA);
        w.writeAttribute(QStringLiteral("xmlns:pic"), kPic);
        w.writeStartElement(QStringLiteral("w:body"));
        const bool lastWasParagraph = writeFrame(w, m_doc->rootFrame());
        if (!lastWasParagraph)
            w.writeEmptyElement(QStringLiteral("w:p"));
        writeSectionProperties(w);
        w.writeEndElement();
        w.writeEndElement();
        w.writeEndDocument();
        return out;
    }

    // Returns true when the last element written was a paragraph.
    bool writeFrame(QXmlStreamWriter &w, QTextFrame *frame)
    {
        bool lastWasParagraph = false;
        for (QTextFrame::iterator it = frame->begin(); !it.atEnd(); ++it) {
            if (QTextFrame *child = it.currentFrame()) {
                if (auto *table = qobject_cast<QTextTable *>(child)) {
                    writeTable(w, table);
                    lastWasParagraph = false;
                } else {
                    lastWasParagraph = writeFrame(w, child);
                }
            } else {
                const QTextBlock block = it.currentBlock();
                if (block.isValid()) {
                    writeBlock(w, block);
                    lastWasParagraph = true;
                }
            }
        }
        return lastWasParagraph;
    }

    ListInfo listInfo(QTextList *list)
    {
        auto it = m_lists.find(list);
        if (it == m_lists.end()) {
            ListInfo info;
            info.numId = m_listOrder.size() + 1;
            info.format = list->format();
            info.level = qBound(0, info.format.indent() - 1, 8);
            m_listOrder << list;
            it = m_lists.insert(list, info);
        }
        return *it;
    }

    void writeParagraphProperties(QXmlStreamWriter &w, const QTextBlock &block)
    {
        const QTextBlockFormat bf = block.blockFormat();
        w.writeStartElement(QStringLiteral("w:pPr"));

        QString styleId = bf.stringProperty(Docx::StyleIdProperty);
        if (styleId.isEmpty() && bf.headingLevel() > 0)
            styleId = QStringLiteral("Heading%1").arg(qMin(bf.headingLevel(), 3));
        if (!styleId.isEmpty() && styleId != QLatin1String("Normal"))
            val(w, QStringLiteral("w:pStyle"), styleId);

        if (bf.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysBefore)
            w.writeEmptyElement(QStringLiteral("w:pageBreakBefore"));

        QTextList *list = block.textList();
        if (list) {
            const ListInfo info = listInfo(list);
            w.writeStartElement(QStringLiteral("w:numPr"));
            val(w, QStringLiteral("w:ilvl"), num(info.level));
            val(w, QStringLiteral("w:numId"), num(info.numId));
            w.writeEndElement();
        }

        if (bf.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            w.writeStartElement(QStringLiteral("w:pBdr"));
            w.writeEmptyElement(QStringLiteral("w:bottom"));
            w.writeAttribute(QStringLiteral("w:val"), QStringLiteral("single"));
            w.writeAttribute(QStringLiteral("w:sz"), QStringLiteral("6"));
            w.writeAttribute(QStringLiteral("w:space"), QStringLiteral("1"));
            w.writeAttribute(QStringLiteral("w:color"), QStringLiteral("auto"));
            w.writeEndElement();
        }

        if (bf.background().style() != Qt::NoBrush) {
            w.writeEmptyElement(QStringLiteral("w:shd"));
            w.writeAttribute(QStringLiteral("w:val"), QStringLiteral("clear"));
            w.writeAttribute(QStringLiteral("w:color"), QStringLiteral("auto"));
            w.writeAttribute(QStringLiteral("w:fill"), hex(bf.background().color()));
        }

        const QList<QTextOption::Tab> tabs = bf.tabPositions();
        if (!tabs.isEmpty()) {
            w.writeStartElement(QStringLiteral("w:tabs"));
            for (const QTextOption::Tab &tab : tabs) {
                w.writeEmptyElement(QStringLiteral("w:tab"));
                QString type = QStringLiteral("left");
                if (tab.type == QTextOption::RightTab)
                    type = QStringLiteral("right");
                else if (tab.type == QTextOption::CenterTab)
                    type = QStringLiteral("center");
                else if (tab.type == QTextOption::DelimiterTab)
                    type = QStringLiteral("decimal");
                w.writeAttribute(QStringLiteral("w:val"), type);
                w.writeAttribute(QStringLiteral("w:pos"), num(twips(tab.position)));
            }
            w.writeEndElement();
        }

        w.writeEmptyElement(QStringLiteral("w:spacing"));
        w.writeAttribute(QStringLiteral("w:before"), num(twips(bf.topMargin())));
        w.writeAttribute(QStringLiteral("w:after"), num(twips(bf.bottomMargin())));
        switch (bf.lineHeightType()) {
        case QTextBlockFormat::ProportionalHeight:
            w.writeAttribute(QStringLiteral("w:line"), num(qRound(bf.lineHeight() * 2.4)));
            w.writeAttribute(QStringLiteral("w:lineRule"), QStringLiteral("auto"));
            break;
        case QTextBlockFormat::FixedHeight:
            w.writeAttribute(QStringLiteral("w:line"), num(twips(bf.lineHeight())));
            w.writeAttribute(QStringLiteral("w:lineRule"), QStringLiteral("exact"));
            break;
        case QTextBlockFormat::MinimumHeight:
            w.writeAttribute(QStringLiteral("w:line"), num(twips(bf.lineHeight())));
            w.writeAttribute(QStringLiteral("w:lineRule"), QStringLiteral("atLeast"));
            break;
        default:
            w.writeAttribute(QStringLiteral("w:line"), QStringLiteral("240"));
            w.writeAttribute(QStringLiteral("w:lineRule"), QStringLiteral("auto"));
            break;
        }

        const qreal left = bf.leftMargin() + (list ? 0 : bf.indent() * m_doc->indentWidth());
        if (!list && (left > 0 || bf.rightMargin() > 0 || bf.textIndent() != 0)) {
            w.writeEmptyElement(QStringLiteral("w:ind"));
            w.writeAttribute(QStringLiteral("w:left"), num(twips(left)));
            w.writeAttribute(QStringLiteral("w:right"), num(twips(bf.rightMargin())));
            if (bf.textIndent() > 0)
                w.writeAttribute(QStringLiteral("w:firstLine"), num(twips(bf.textIndent())));
            else if (bf.textIndent() < 0)
                w.writeAttribute(QStringLiteral("w:hanging"), num(twips(-bf.textIndent())));
        }

        const Qt::Alignment a = bf.alignment();
        if (a & Qt::AlignHCenter)
            val(w, QStringLiteral("w:jc"), QStringLiteral("center"));
        else if (a & Qt::AlignRight)
            val(w, QStringLiteral("w:jc"), QStringLiteral("right"));
        else if (a & Qt::AlignJustify)
            val(w, QStringLiteral("w:jc"), QStringLiteral("both"));

        if (block.length() <= 1)
            writeRunProperties(w, block.charFormat(), false);
        w.writeEndElement();
    }

    void writeRunProperties(QXmlStreamWriter &w, const QTextCharFormat &cf, bool link)
    {
        w.writeStartElement(QStringLiteral("w:rPr"));
        if (link)
            val(w, QStringLiteral("w:rStyle"), QStringLiteral("Hyperlink"));
        const QStringList families = cf.property(QTextFormat::FontFamilies).toStringList();
        if (!families.isEmpty() && !families.first().isEmpty()) {
            w.writeEmptyElement(QStringLiteral("w:rFonts"));
            for (const char *attr : {"w:ascii", "w:hAnsi", "w:cs", "w:eastAsia"})
                w.writeAttribute(QString::fromLatin1(attr), families.first());
        }
        if (cf.hasProperty(QTextFormat::FontWeight)) {
            if (cf.fontWeight() >= QFont::DemiBold)
                w.writeEmptyElement(QStringLiteral("w:b"));
            else
                val(w, QStringLiteral("w:b"), QStringLiteral("0"));
        }
        if (cf.hasProperty(QTextFormat::FontItalic)) {
            if (cf.fontItalic())
                w.writeEmptyElement(QStringLiteral("w:i"));
            else
                val(w, QStringLiteral("w:i"), QStringLiteral("0"));
        }
        if (cf.hasProperty(QTextFormat::FontCapitalization)) {
            if (cf.fontCapitalization() == QFont::AllUppercase)
                w.writeEmptyElement(QStringLiteral("w:caps"));
            else if (cf.fontCapitalization() == QFont::SmallCaps)
                w.writeEmptyElement(QStringLiteral("w:smallCaps"));
        }
        if (cf.fontStrikeOut())
            w.writeEmptyElement(QStringLiteral("w:strike"));
        if (cf.hasProperty(QTextFormat::ForegroundBrush) && cf.foreground().style() != Qt::NoBrush)
            val(w, QStringLiteral("w:color"), hex(cf.foreground().color()));
        if (cf.hasProperty(QTextFormat::FontPointSize) && cf.fontPointSize() > 0) {
            const QString size = num(qRound(cf.fontPointSize() * 2));
            val(w, QStringLiteral("w:sz"), size);
            val(w, QStringLiteral("w:szCs"), size);
        }
        QString shading;
        if (cf.background().style() != Qt::NoBrush) {
            const QString name = highlightName(cf.background().color());
            if (!name.isEmpty())
                val(w, QStringLiteral("w:highlight"), name);
            else
                shading = hex(cf.background().color());
        }
        if (cf.hasProperty(QTextFormat::TextUnderlineStyle)) {
            QString u;
            switch (cf.underlineStyle()) {
            case QTextCharFormat::NoUnderline: u = QStringLiteral("none"); break;
            case QTextCharFormat::DotLine: u = QStringLiteral("dotted"); break;
            case QTextCharFormat::DashUnderline: u = QStringLiteral("dash"); break;
            case QTextCharFormat::DashDotLine: u = QStringLiteral("dotDash"); break;
            case QTextCharFormat::DashDotDotLine: u = QStringLiteral("dotDotDash"); break;
            case QTextCharFormat::WaveUnderline:
            case QTextCharFormat::SpellCheckUnderline: u = QStringLiteral("wave"); break;
            default: u = QStringLiteral("single"); break;
            }
            if (!(link && u == QLatin1String("single")))
                val(w, QStringLiteral("w:u"), u);
        }
        if (!shading.isEmpty()) {
            w.writeEmptyElement(QStringLiteral("w:shd"));
            w.writeAttribute(QStringLiteral("w:val"), QStringLiteral("clear"));
            w.writeAttribute(QStringLiteral("w:color"), QStringLiteral("auto"));
            w.writeAttribute(QStringLiteral("w:fill"), shading);
        }
        if (cf.verticalAlignment() == QTextCharFormat::AlignSuperScript)
            val(w, QStringLiteral("w:vertAlign"), QStringLiteral("superscript"));
        else if (cf.verticalAlignment() == QTextCharFormat::AlignSubScript)
            val(w, QStringLiteral("w:vertAlign"), QStringLiteral("subscript"));
        w.writeEndElement();
    }

    void writeRun(QXmlStreamWriter &w, const QString &text, const QTextCharFormat &cf, bool link)
    {
        w.writeStartElement(QStringLiteral("w:r"));
        writeRunProperties(w, cf, link);
        QString buffer;
        auto flush = [&] {
            if (buffer.isEmpty())
                return;
            w.writeStartElement(QStringLiteral("w:t"));
            w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
            w.writeCharacters(buffer);
            w.writeEndElement();
            buffer.clear();
        };
        for (const QChar ch : text) {
            if (ch == QLatin1Char('\t')) {
                flush();
                w.writeEmptyElement(QStringLiteral("w:tab"));
            } else if (ch == QChar::LineSeparator || ch == QLatin1Char('\n')) {
                flush();
                w.writeEmptyElement(QStringLiteral("w:br"));
            } else if (ch == QChar::ObjectReplacementCharacter || ch == QChar::ParagraphSeparator
                       || ch.unicode() < 0x20 || ch.unicode() == 0xFFFE || ch.unicode() == 0xFFFF) {
                continue;
            } else {
                buffer += ch;
            }
        }
        flush();
        w.writeEndElement();
    }

    QImage imageFor(const QString &name) const
    {
        const QVariant res = m_doc->resource(QTextDocument::ImageResource, QUrl(name));
        QImage image;
        if (res.canConvert<QImage>())
            image = res.value<QImage>();
        else if (res.canConvert<QPixmap>())
            image = res.value<QPixmap>().toImage();
        else if (res.typeId() == QMetaType::QByteArray)
            image.loadFromData(res.toByteArray());
        if (image.isNull()) {
            const QUrl url(name);
            image.load(url.isLocalFile() ? url.toLocalFile() : name);
        }
        return image;
    }

    void writeImage(QXmlStreamWriter &w, const QTextImageFormat &format)
    {
        const QString name = format.name();
        const QImage image = imageFor(name);
        if (image.isNull())
            return;
        QString relId = m_imageRels.value(name);
        if (relId.isEmpty()) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            QString ext = QStringLiteral("png");
            if (!image.hasAlphaChannel() && qint64(image.width()) * image.height() > 250000) {
                image.save(&buffer, "JPEG", 90);
                ext = QStringLiteral("jpeg");
            } else {
                image.save(&buffer, "PNG");
            }
            const QString file = QStringLiteral("image%1.%2").arg(m_media.size() + 1).arg(ext);
            m_media << qMakePair(file, bytes);
            relId = m_rels.add(rel("image"), QStringLiteral("media/") + file);
            m_imageRels.insert(name, relId);
        }
        qreal wpx = format.width() > 0 ? format.width() : image.width();
        qreal hpx = format.height() > 0 ? format.height() : image.height();
        if (format.width() > 0 && format.height() <= 0)
            hpx = image.height() * wpx / qMax(1, image.width());
        const QString cx = num(emu(wpx));
        const QString cy = num(emu(hpx));
        const int id = m_drawingId++;

        w.writeStartElement(QStringLiteral("w:r"));
        w.writeStartElement(QStringLiteral("w:drawing"));
        w.writeStartElement(QStringLiteral("wp:inline"));
        for (const char *d : {"distT", "distB", "distL", "distR"})
            w.writeAttribute(QString::fromLatin1(d), QStringLiteral("0"));
        w.writeEmptyElement(QStringLiteral("wp:extent"));
        w.writeAttribute(QStringLiteral("cx"), cx);
        w.writeAttribute(QStringLiteral("cy"), cy);
        w.writeEmptyElement(QStringLiteral("wp:docPr"));
        w.writeAttribute(QStringLiteral("id"), num(id));
        w.writeAttribute(QStringLiteral("name"), QStringLiteral("Resim %1").arg(id));
        w.writeStartElement(QStringLiteral("wp:cNvGraphicFramePr"));
        w.writeEmptyElement(QStringLiteral("a:graphicFrameLocks"));
        w.writeAttribute(QStringLiteral("noChangeAspect"), QStringLiteral("1"));
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("a:graphic"));
        w.writeStartElement(QStringLiteral("a:graphicData"));
        w.writeAttribute(QStringLiteral("uri"), kPic);
        w.writeStartElement(QStringLiteral("pic:pic"));
        w.writeStartElement(QStringLiteral("pic:nvPicPr"));
        w.writeEmptyElement(QStringLiteral("pic:cNvPr"));
        w.writeAttribute(QStringLiteral("id"), num(id));
        w.writeAttribute(QStringLiteral("name"), QStringLiteral("Resim %1").arg(id));
        w.writeEmptyElement(QStringLiteral("pic:cNvPicPr"));
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("pic:blipFill"));
        w.writeEmptyElement(QStringLiteral("a:blip"));
        w.writeAttribute(QStringLiteral("r:embed"), relId);
        w.writeStartElement(QStringLiteral("a:stretch"));
        w.writeEmptyElement(QStringLiteral("a:fillRect"));
        w.writeEndElement();
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("pic:spPr"));
        w.writeStartElement(QStringLiteral("a:xfrm"));
        w.writeEmptyElement(QStringLiteral("a:off"));
        w.writeAttribute(QStringLiteral("x"), QStringLiteral("0"));
        w.writeAttribute(QStringLiteral("y"), QStringLiteral("0"));
        w.writeEmptyElement(QStringLiteral("a:ext"));
        w.writeAttribute(QStringLiteral("cx"), cx);
        w.writeAttribute(QStringLiteral("cy"), cy);
        w.writeEndElement();
        w.writeStartElement(QStringLiteral("a:prstGeom"));
        w.writeAttribute(QStringLiteral("prst"), QStringLiteral("rect"));
        w.writeEmptyElement(QStringLiteral("a:avLst"));
        w.writeEndElement();
        w.writeEndElement(); // spPr
        w.writeEndElement(); // pic
        w.writeEndElement(); // graphicData
        w.writeEndElement(); // graphic
        w.writeEndElement(); // inline
        w.writeEndElement(); // drawing
        w.writeEndElement(); // r
    }

    void writeBlock(QXmlStreamWriter &w, const QTextBlock &block)
    {
        w.writeStartElement(QStringLiteral("w:p"));
        writeParagraphProperties(w, block);

        const QTextBlockFormat bf = block.blockFormat();
        if (bf.marker() != QTextBlockFormat::MarkerType::NoMarker) {
            const QString box = bf.marker() == QTextBlockFormat::MarkerType::Checked ? QStringLiteral("☒ ")
                                                                                    : QStringLiteral("☐ ");
            writeRun(w, box, block.charFormat(), false);
        }

        QString openHref;
        auto closeLink = [&] {
            if (!openHref.isEmpty()) {
                w.writeEndElement();
                openHref.clear();
            }
        };
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (!frag.isValid())
                continue;
            const QTextCharFormat cf = frag.charFormat();
            const QString href = cf.isAnchor() ? cf.anchorHref() : QString();
            if (href != openHref) {
                closeLink();
                if (!href.isEmpty()) {
                    w.writeStartElement(QStringLiteral("w:hyperlink"));
                    if (href.startsWith(QLatin1Char('#')))
                        w.writeAttribute(QStringLiteral("w:anchor"), href.mid(1));
                    else
                        w.writeAttribute(QStringLiteral("r:id"), m_rels.add(rel("hyperlink"), href, true));
                    w.writeAttribute(QStringLiteral("w:history"), QStringLiteral("1"));
                    openHref = href;
                }
            }
            QList<int> bookmarks;
            const QStringList names = cf.anchorNames();
            for (const QString &name : names) {
                const int id = m_bookmarkId++;
                bookmarks << id;
                w.writeEmptyElement(QStringLiteral("w:bookmarkStart"));
                w.writeAttribute(QStringLiteral("w:id"), num(id));
                w.writeAttribute(QStringLiteral("w:name"), name);
            }
            if (cf.isImageFormat()) {
                for (int i = 0; i < frag.length(); ++i)
                    writeImage(w, cf.toImageFormat());
            } else {
                writeRun(w, frag.text(), cf, !href.isEmpty());
            }
            for (const int id : std::as_const(bookmarks)) {
                w.writeEmptyElement(QStringLiteral("w:bookmarkEnd"));
                w.writeAttribute(QStringLiteral("w:id"), num(id));
            }
        }
        closeLink();
        w.writeEndElement();
    }

    void writeTable(QXmlStreamWriter &w, QTextTable *table)
    {
        const QTextTableFormat tf = table->format();
        const int rows = table->rows();
        const int cols = table->columns();

        int tableTwips = m_textWidthTwips;
        if (tf.width().type() == QTextLength::FixedLength)
            tableTwips = twips(tf.width().rawValue());
        else if (tf.width().type() == QTextLength::PercentageLength)
            tableTwips = qRound(m_textWidthTwips * tf.width().rawValue() / 100.0);

        QList<int> widths(cols, 0);
        const QList<QTextLength> constraints = tf.columnWidthConstraints();
        int assigned = 0;
        int unassigned = 0;
        for (int c = 0; c < cols; ++c) {
            if (c < constraints.size()) {
                const QTextLength l = constraints.at(c);
                if (l.type() == QTextLength::PercentageLength)
                    widths[c] = qRound(tableTwips * l.rawValue() / 100.0);
                else if (l.type() == QTextLength::FixedLength)
                    widths[c] = twips(l.rawValue());
            }
            if (widths[c] > 0)
                assigned += widths[c];
            else
                ++unassigned;
        }
        if (unassigned > 0) {
            const int each = qMax(200, (tableTwips - assigned) / unassigned);
            for (int &wdt : widths) {
                if (wdt <= 0)
                    wdt = each;
            }
        }

        w.writeStartElement(QStringLiteral("w:tbl"));
        w.writeStartElement(QStringLiteral("w:tblPr"));
        if (tf.border() > 0)
            val(w, QStringLiteral("w:tblStyle"), QStringLiteral("TableGrid"));
        w.writeEmptyElement(QStringLiteral("w:tblW"));
        w.writeAttribute(QStringLiteral("w:w"), num(tableTwips));
        w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("dxa"));
        if (tf.alignment() & Qt::AlignHCenter)
            val(w, QStringLiteral("w:jc"), QStringLiteral("center"));
        if (tf.border() > 0) {
            const QString color = tf.borderBrush().style() != Qt::NoBrush ? hex(tf.borderBrush().color())
                                                                          : QStringLiteral("auto");
            const QString size = num(qMax(2, qRound(tf.border() * 6)));
            w.writeStartElement(QStringLiteral("w:tblBorders"));
            for (const char *side : {"w:top", "w:left", "w:bottom", "w:right", "w:insideH", "w:insideV"}) {
                w.writeEmptyElement(QString::fromLatin1(side));
                w.writeAttribute(QStringLiteral("w:val"), QStringLiteral("single"));
                w.writeAttribute(QStringLiteral("w:sz"), size);
                w.writeAttribute(QStringLiteral("w:space"), QStringLiteral("0"));
                w.writeAttribute(QStringLiteral("w:color"), color);
            }
            w.writeEndElement();
        }
        w.writeStartElement(QStringLiteral("w:tblCellMar"));
        w.writeEmptyElement(QStringLiteral("w:left"));
        w.writeAttribute(QStringLiteral("w:w"), num(twips(tf.cellPadding())));
        w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("dxa"));
        w.writeEmptyElement(QStringLiteral("w:right"));
        w.writeAttribute(QStringLiteral("w:w"), num(twips(tf.cellPadding())));
        w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("dxa"));
        w.writeEndElement();
        w.writeEndElement(); // tblPr

        w.writeStartElement(QStringLiteral("w:tblGrid"));
        for (const int wdt : std::as_const(widths)) {
            w.writeEmptyElement(QStringLiteral("w:gridCol"));
            w.writeAttribute(QStringLiteral("w:w"), num(wdt));
        }
        w.writeEndElement();

        for (int r = 0; r < rows; ++r) {
            w.writeStartElement(QStringLiteral("w:tr"));
            if (r < tf.headerRowCount()) {
                w.writeStartElement(QStringLiteral("w:trPr"));
                w.writeEmptyElement(QStringLiteral("w:tblHeader"));
                w.writeEndElement();
            }
            for (int c = 0; c < cols; ++c) {
                const QTextTableCell cell = table->cellAt(r, c);
                if (!cell.isValid() || cell.column() != c)
                    continue;
                const bool continuation = cell.row() != r;
                int spanWidth = 0;
                for (int k = c; k < qMin(cols, c + cell.columnSpan()); ++k)
                    spanWidth += widths.at(k);

                w.writeStartElement(QStringLiteral("w:tc"));
                w.writeStartElement(QStringLiteral("w:tcPr"));
                w.writeEmptyElement(QStringLiteral("w:tcW"));
                w.writeAttribute(QStringLiteral("w:w"), num(spanWidth));
                w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("dxa"));
                if (cell.columnSpan() > 1)
                    val(w, QStringLiteral("w:gridSpan"), num(cell.columnSpan()));
                if (continuation)
                    w.writeEmptyElement(QStringLiteral("w:vMerge"));
                else if (cell.rowSpan() > 1)
                    val(w, QStringLiteral("w:vMerge"), QStringLiteral("restart"));
                const QTextTableCellFormat cellFormat = cell.format().toTableCellFormat();
                if (cellFormat.background().style() != Qt::NoBrush) {
                    w.writeEmptyElement(QStringLiteral("w:shd"));
                    w.writeAttribute(QStringLiteral("w:val"), QStringLiteral("clear"));
                    w.writeAttribute(QStringLiteral("w:color"), QStringLiteral("auto"));
                    w.writeAttribute(QStringLiteral("w:fill"), hex(cellFormat.background().color()));
                }
                if (cellFormat.verticalAlignment() == QTextCharFormat::AlignMiddle)
                    val(w, QStringLiteral("w:vAlign"), QStringLiteral("center"));
                else if (cellFormat.verticalAlignment() == QTextCharFormat::AlignBottom)
                    val(w, QStringLiteral("w:vAlign"), QStringLiteral("bottom"));
                w.writeEndElement(); // tcPr

                if (continuation) {
                    w.writeEmptyElement(QStringLiteral("w:p"));
                } else {
                    bool lastWasParagraph = false;
                    for (QTextFrame::iterator it = cell.begin(); !it.atEnd(); ++it) {
                        if (QTextFrame *child = it.currentFrame()) {
                            if (auto *nested = qobject_cast<QTextTable *>(child)) {
                                writeTable(w, nested);
                                lastWasParagraph = false;
                            } else {
                                lastWasParagraph = writeFrame(w, child);
                            }
                        } else if (it.currentBlock().isValid()) {
                            writeBlock(w, it.currentBlock());
                            lastWasParagraph = true;
                        }
                    }
                    if (!lastWasParagraph)
                        w.writeEmptyElement(QStringLiteral("w:p"));
                }
                w.writeEndElement(); // tc
            }
            w.writeEndElement(); // tr
        }
        w.writeEndElement(); // tbl
    }

    void writeSectionProperties(QXmlStreamWriter &w)
    {
        w.writeStartElement(QStringLiteral("w:sectPr"));
        if (!m_headerRel.isEmpty()) {
            w.writeEmptyElement(QStringLiteral("w:headerReference"));
            w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("default"));
            w.writeAttribute(QStringLiteral("r:id"), m_headerRel);
        }
        if (!m_footerRel.isEmpty()) {
            w.writeEmptyElement(QStringLiteral("w:footerReference"));
            w.writeAttribute(QStringLiteral("w:type"), QStringLiteral("default"));
            w.writeAttribute(QStringLiteral("r:id"), m_footerRel);
        }
        const QSizeF mm = m_page.sizeMm();
        w.writeEmptyElement(QStringLiteral("w:pgSz"));
        w.writeAttribute(QStringLiteral("w:w"), num(mmToTwips(mm.width())));
        w.writeAttribute(QStringLiteral("w:h"), num(mmToTwips(mm.height())));
        if (m_page.orientation == QPageLayout::Landscape)
            w.writeAttribute(QStringLiteral("w:orient"), QStringLiteral("landscape"));
        w.writeEmptyElement(QStringLiteral("w:pgMar"));
        w.writeAttribute(QStringLiteral("w:top"), num(mmToTwips(m_page.margins.top())));
        w.writeAttribute(QStringLiteral("w:right"), num(mmToTwips(m_page.margins.right())));
        w.writeAttribute(QStringLiteral("w:bottom"), num(mmToTwips(m_page.margins.bottom())));
        w.writeAttribute(QStringLiteral("w:left"), num(mmToTwips(m_page.margins.left())));
        w.writeAttribute(QStringLiteral("w:header"), QStringLiteral("708"));
        w.writeAttribute(QStringLiteral("w:footer"), QStringLiteral("708"));
        w.writeAttribute(QStringLiteral("w:gutter"), QStringLiteral("0"));
        w.writeEmptyElement(QStringLiteral("w:cols"));
        w.writeAttribute(QStringLiteral("w:space"), QStringLiteral("708"));
        w.writeEmptyElement(QStringLiteral("w:docGrid"));
        w.writeAttribute(QStringLiteral("w:linePitch"), QStringLiteral("360"));
        w.writeEndElement();
    }

    QByteArray numberingXml() const
    {
        QByteArray out;
        QXmlStreamWriter w(&out);
        w.writeStartDocument(QStringLiteral("1.0"), true);
        w.writeStartElement(QStringLiteral("w:numbering"));
        w.writeAttribute(QStringLiteral("xmlns:w"), kW);
        for (int i = 0; i < m_listOrder.size(); ++i) {
            const ListInfo info = m_lists.value(m_listOrder.at(i));
            const QTextListFormat lf = info.format;
            QString numFmt = QStringLiteral("decimal");
            QString bullet;
            switch (lf.style()) {
            case QTextListFormat::ListDisc: numFmt = QStringLiteral("bullet"); bullet = QStringLiteral("•"); break;
            case QTextListFormat::ListCircle: numFmt = QStringLiteral("bullet"); bullet = QStringLiteral("◦"); break;
            case QTextListFormat::ListSquare: numFmt = QStringLiteral("bullet"); bullet = QStringLiteral("▪"); break;
            case QTextListFormat::ListLowerAlpha: numFmt = QStringLiteral("lowerLetter"); break;
            case QTextListFormat::ListUpperAlpha: numFmt = QStringLiteral("upperLetter"); break;
            case QTextListFormat::ListLowerRoman: numFmt = QStringLiteral("lowerRoman"); break;
            case QTextListFormat::ListUpperRoman: numFmt = QStringLiteral("upperRoman"); break;
            default: break;
            }
            w.writeStartElement(QStringLiteral("w:abstractNum"));
            w.writeAttribute(QStringLiteral("w:abstractNumId"), num(i));
            val(w, QStringLiteral("w:multiLevelType"), QStringLiteral("hybridMultilevel"));
            for (int lvl = 0; lvl < 9; ++lvl) {
                w.writeStartElement(QStringLiteral("w:lvl"));
                w.writeAttribute(QStringLiteral("w:ilvl"), num(lvl));
                val(w, QStringLiteral("w:start"), num(qMax(1, lf.start())));
                val(w, QStringLiteral("w:numFmt"), numFmt);
                val(w, QStringLiteral("w:lvlText"), bullet.isEmpty() ? lf.numberPrefix() + QLatin1Char('%')
                                                                         + num(lvl + 1) + lf.numberSuffix()
                                                                   : bullet);
                val(w, QStringLiteral("w:lvlJc"), QStringLiteral("left"));
                w.writeStartElement(QStringLiteral("w:pPr"));
                w.writeEmptyElement(QStringLiteral("w:ind"));
                w.writeAttribute(QStringLiteral("w:left"), num(720 * (lvl + 1)));
                w.writeAttribute(QStringLiteral("w:hanging"), QStringLiteral("360"));
                w.writeEndElement();
                w.writeEndElement();
            }
            w.writeEndElement();
        }
        for (int i = 0; i < m_listOrder.size(); ++i) {
            w.writeStartElement(QStringLiteral("w:num"));
            w.writeAttribute(QStringLiteral("w:numId"), num(i + 1));
            val(w, QStringLiteral("w:abstractNumId"), num(i));
            w.writeEndElement();
        }
        w.writeEndElement();
        w.writeEndDocument();
        return out;
    }

    QByteArray headerFooterXml(bool header, const QString &text) const
    {
        QByteArray out;
        QXmlStreamWriter w(&out);
        w.writeStartDocument(QStringLiteral("1.0"), true);
        w.writeStartElement(header ? QStringLiteral("w:hdr") : QStringLiteral("w:ftr"));
        w.writeAttribute(QStringLiteral("xmlns:w"), kW);
        w.writeAttribute(QStringLiteral("xmlns:r"), kR);
        w.writeStartElement(QStringLiteral("w:p"));
        w.writeStartElement(QStringLiteral("w:pPr"));
        w.writeEmptyElement(QStringLiteral("w:spacing"));
        w.writeAttribute(QStringLiteral("w:after"), QStringLiteral("0"));
        val(w, QStringLiteral("w:jc"), QStringLiteral("center"));
        w.writeEndElement();

        auto textRun = [&w](const QString &t) {
            if (t.isEmpty())
                return;
            w.writeStartElement(QStringLiteral("w:r"));
            w.writeStartElement(QStringLiteral("w:t"));
            w.writeAttribute(QStringLiteral("xml:space"), QStringLiteral("preserve"));
            w.writeCharacters(t);
            w.writeEndElement();
            w.writeEndElement();
        };
        auto field = [&w](const QString &instr, const QString &placeholder) {
            w.writeStartElement(QStringLiteral("w:fldSimple"));
            w.writeAttribute(QStringLiteral("w:instr"), instr);
            w.writeStartElement(QStringLiteral("w:r"));
            w.writeTextElement(QStringLiteral("w:t"), placeholder);
            w.writeEndElement();
            w.writeEndElement();
        };
        static const QRegularExpression token(QStringLiteral("\\{(page|pages|date)\\}"));
        int pos = 0;
        QRegularExpressionMatchIterator it = token.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            textRun(text.mid(pos, m.capturedStart() - pos));
            const QString t = m.captured(1);
            if (t == QLatin1String("page"))
                field(QStringLiteral(" PAGE "), QStringLiteral("1"));
            else if (t == QLatin1String("pages"))
                field(QStringLiteral(" NUMPAGES "), QStringLiteral("1"));
            else
                field(QStringLiteral(" DATE \\@ \"dd.MM.yyyy\" "), QDate::currentDate().toString(QStringLiteral("dd.MM.yyyy")));
            pos = m.capturedEnd();
        }
        textRun(text.mid(pos));
        w.writeEndElement(); // p
        w.writeEndElement(); // hdr/ftr
        w.writeEndDocument();
        return out;
    }

    QByteArray contentTypes() const
    {
        QByteArray out;
        QXmlStreamWriter w(&out);
        w.writeStartDocument(QStringLiteral("1.0"), true);
        w.writeStartElement(QStringLiteral("Types"));
        w.writeDefaultNamespace(QStringLiteral("http://schemas.openxmlformats.org/package/2006/content-types"));
        auto def = [&w](const char *ext, const char *type) {
            w.writeEmptyElement(QStringLiteral("Default"));
            w.writeAttribute(QStringLiteral("Extension"), QString::fromLatin1(ext));
            w.writeAttribute(QStringLiteral("ContentType"), QString::fromLatin1(type));
        };
        auto over = [&w](const char *part, const char *type) {
            w.writeEmptyElement(QStringLiteral("Override"));
            w.writeAttribute(QStringLiteral("PartName"), QString::fromLatin1(part));
            w.writeAttribute(QStringLiteral("ContentType"), QString::fromLatin1(type));
        };
        def("rels", "application/vnd.openxmlformats-package.relationships+xml");
        def("xml", "application/xml");
        def("png", "image/png");
        def("jpeg", "image/jpeg");
        over("/word/document.xml", "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
        over("/word/styles.xml", "application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml");
        over("/word/settings.xml", "application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml");
        over("/word/numbering.xml", "application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml");
        if (!m_headerRel.isEmpty())
            over("/word/header1.xml", "application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml");
        if (!m_footerRel.isEmpty())
            over("/word/footer1.xml", "application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml");
        over("/docProps/core.xml", "application/vnd.openxmlformats-package.core-properties+xml");
        over("/docProps/app.xml", "application/vnd.openxmlformats-officedocument.extended-properties+xml");
        w.writeEndElement();
        w.writeEndDocument();
        return out;
    }

    static QByteArray rootRels()
    {
        Ooxml::RelationshipWriter r;
        r.add(QString::fromLatin1(Ooxml::kOfficeDocumentRel), QStringLiteral("word/document.xml"));
        r.add(QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"),
              QStringLiteral("docProps/core.xml"));
        r.add(rel("extended-properties"), QStringLiteral("docProps/app.xml"));
        return r.toXml();
    }

    const QTextDocument *m_doc;
    PageSetup m_page;
    DocxMetadata m_meta;
    Ooxml::RelationshipWriter m_rels;
    QHash<QTextList *, ListInfo> m_lists;
    QList<QTextList *> m_listOrder;
    QList<QPair<QString, QByteArray>> m_media;
    QHash<QString, QString> m_imageRels;
    QString m_headerRel;
    QString m_footerRel;
    int m_textWidthTwips = 9000;
    int m_drawingId = 1;
    int m_bookmarkId = 0;
};

} // namespace

namespace Docx {

QByteArray toByteArray(const QTextDocument *document, const PageSetup &page, const DocxMetadata &meta)
{
    DocxBuilder builder(document, page, meta);
    return builder.build();
}

bool write(const QString &path, const QTextDocument *document, const PageSetup &page, const DocxMetadata &meta,
           QString *errorMessage)
{
    const QByteArray data = toByteArray(document, page, meta);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("Docx", "Dosya yazılamadı: %1").arg(file.errorString());
        return false;
    }
    return true;
}

} // namespace Docx
