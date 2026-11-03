#include "formats/Docx.h"

#include "formats/Ooxml.h"
#include "formats/Zip.h"

#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextTable>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamReader>

using Ooxml::attribute;

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Docx", text);
}

qreal twipsToPx(const QString &v) { return v.toDouble() / 15.0; }

QColor hexColor(const QString &v)
{
    if (v.isEmpty() || v.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0)
        return {};
    const QColor c(QLatin1Char('#') + v);
    return c.isValid() ? c : QColor();
}

QColor highlightColor(const QString &name)
{
    static const QHash<QString, QColor> colors = {
        {QStringLiteral("yellow"), QColor(0xFF, 0xFF, 0x00)},   {QStringLiteral("green"), QColor(0x00, 0xFF, 0x00)},
        {QStringLiteral("cyan"), QColor(0x00, 0xFF, 0xFF)},     {QStringLiteral("magenta"), QColor(0xFF, 0x00, 0xFF)},
        {QStringLiteral("blue"), QColor(0x00, 0x00, 0xFF)},     {QStringLiteral("red"), QColor(0xFF, 0x00, 0x00)},
        {QStringLiteral("darkBlue"), QColor(0x00, 0x00, 0x80)}, {QStringLiteral("darkCyan"), QColor(0x00, 0x80, 0x80)},
        {QStringLiteral("darkGreen"), QColor(0x00, 0x80, 0x00)}, {QStringLiteral("darkMagenta"), QColor(0x80, 0x00, 0x80)},
        {QStringLiteral("darkRed"), QColor(0x80, 0x00, 0x00)},  {QStringLiteral("darkYellow"), QColor(0x80, 0x80, 0x00)},
        {QStringLiteral("darkGray"), QColor(0x80, 0x80, 0x80)}, {QStringLiteral("lightGray"), QColor(0xC0, 0xC0, 0xC0)},
        {QStringLiteral("black"), QColor(0x00, 0x00, 0x00)},    {QStringLiteral("white"), QColor(0xFF, 0xFF, 0xFF)}};
    return colors.value(name);
}

struct ParaProps
{
    QTextBlockFormat bf;
    QTextCharFormat markCf;
    QString styleId;
    QString numId;
    int ilvl = -1;
    int outlineLevel = 0;
    bool pageBreakBefore = false;
};

struct StyleDef
{
    QString id;
    QString name;
    QString type;
    QString basedOn;
    QTextBlockFormat bf;
    QTextCharFormat cf;
    int outlineLevel = 0;
    QString numId;
    int ilvl = 0;
};

struct NumLevel
{
    QString format;
    QString text;
    int start = 1;
};

class DocxParser
{
public:
    explicit DocxParser(QTextDocument *doc) : m_doc(doc) {}

    bool parse(const ZipReader &zip, PageSetup *page, DocxMetadata *meta, QString *errorMessage)
    {
        m_zip = &zip;
        m_docPath = Ooxml::mainPartPath(zip, QStringLiteral("word/document.xml"));
        bool ok = false;
        const QByteArray xml = zip.read(m_docPath, &ok);
        if (!ok) {
            if (errorMessage)
                *errorMessage = tr("Belge içeriği bulunamadı. Dosya bir Word belgesi olmayabilir.");
            return false;
        }
        m_rels = Ooxml::readRelationships(zip, m_docPath);
        loadTheme();
        loadStyles();
        loadNumbering();

        m_doc->clear();
        m_doc->setUndoRedoEnabled(false);
        QFont defaultFont(m_defaultCf.property(QTextFormat::FontFamilies).toStringList().value(0, QStringLiteral("Calibri")));
        defaultFont.setPointSizeF(m_defaultCf.fontPointSize() > 0 ? m_defaultCf.fontPointSize() : 11);
        m_doc->setDefaultFont(defaultFont);
        m_doc->setIndentWidth(48);
        m_cursor = QTextCursor(m_doc);
        m_reuseBlock = true;

        QXmlStreamReader r(xml);
        if (r.readNextStartElement()) {
            while (r.readNextStartElement()) {
                if (r.name() == QLatin1String("body"))
                    parseBlockContent(r);
                else
                    r.skipCurrentElement();
            }
        }
        if (r.hasError() && r.error() != QXmlStreamReader::PrematureEndOfDocumentError) {
            if (errorMessage)
                *errorMessage = tr("Belge çözümlenemedi: %1").arg(r.errorString());
            m_doc->setUndoRedoEnabled(true);
            return false;
        }

        convertCheckboxes();
        m_page.header = headerFooterText(m_headerRid);
        m_page.footer = headerFooterText(m_footerRid);
        if (page)
            *page = m_page;
        if (meta)
            readCoreProperties(meta);
        m_doc->setUndoRedoEnabled(true);
        m_doc->clearUndoRedoStacks();
        m_doc->setModified(false);
        return true;
    }

private:
    // ---- package parts ----------------------------------------------------------

    QString partFor(const char *relType, const QString &fallback) const
    {
        for (const Ooxml::Relationship &rel : m_rels) {
            if (rel.type.endsWith(QLatin1Char('/') + QLatin1String(relType)))
                return rel.target;
        }
        return fallback;
    }

    void loadTheme()
    {
        const QByteArray data = m_zip->read(partFor("theme", QStringLiteral("word/theme/theme1.xml")));
        if (data.isEmpty())
            return;
        QXmlStreamReader r(data);
        QString current;
        while (!r.atEnd()) {
            if (r.readNext() != QXmlStreamReader::StartElement)
                continue;
            if (r.name() == QLatin1String("majorFont") || r.name() == QLatin1String("minorFont")) {
                current = r.name().toString();
            } else if (r.name() == QLatin1String("latin") && !current.isEmpty()) {
                const QString face = attribute(r, "typeface");
                if (current == QLatin1String("majorFont") && m_majorFont.isEmpty())
                    m_majorFont = face;
                else if (current == QLatin1String("minorFont") && m_minorFont.isEmpty())
                    m_minorFont = face;
                current.clear();
            }
        }
    }

    void loadStyles()
    {
        const QByteArray data = m_zip->read(partFor("styles", QStringLiteral("word/styles.xml")));
        if (data.isEmpty())
            return;
        QXmlStreamReader r(data);
        if (!r.readNextStartElement())
            return;
        while (r.readNextStartElement()) {
            if (r.name() == QLatin1String("docDefaults")) {
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("rPrDefault") || r.name() == QLatin1String("pPrDefault")) {
                        while (r.readNextStartElement()) {
                            if (r.name() == QLatin1String("rPr")) {
                                parseRunProperties(r, m_defaultCf, nullptr);
                            } else if (r.name() == QLatin1String("pPr")) {
                                ParaProps p;
                                parseParagraphProperties(r, p);
                                m_defaultBf = p.bf;
                            } else {
                                r.skipCurrentElement();
                            }
                        }
                    } else {
                        r.skipCurrentElement();
                    }
                }
            } else if (r.name() == QLatin1String("style")) {
                StyleDef s;
                s.type = attribute(r, "type");
                s.id = attribute(r, "styleId");
                const bool isDefault = attribute(r, "default") == QLatin1String("1")
                                       || attribute(r, "default") == QLatin1String("true");
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("name")) {
                        s.name = attribute(r, "val");
                        r.skipCurrentElement();
                    } else if (r.name() == QLatin1String("basedOn")) {
                        s.basedOn = attribute(r, "val");
                        r.skipCurrentElement();
                    } else if (r.name() == QLatin1String("pPr")) {
                        ParaProps p;
                        parseParagraphProperties(r, p);
                        s.bf = p.bf;
                        s.outlineLevel = p.outlineLevel;
                        s.numId = p.numId;
                        s.ilvl = qMax(0, p.ilvl);
                    } else if (r.name() == QLatin1String("rPr")) {
                        parseRunProperties(r, s.cf, nullptr);
                    } else {
                        r.skipCurrentElement();
                    }
                }
                if (isDefault && s.type == QLatin1String("paragraph"))
                    m_defaultParagraphStyle = s.id;
                m_styles.insert(s.id, s);
            } else {
                r.skipCurrentElement();
            }
        }
    }

    void loadNumbering()
    {
        const QByteArray data = m_zip->read(partFor("numbering", QStringLiteral("word/numbering.xml")));
        if (data.isEmpty())
            return;
        QXmlStreamReader r(data);
        if (!r.readNextStartElement())
            return;
        while (r.readNextStartElement()) {
            if (r.name() == QLatin1String("abstractNum")) {
                const QString id = attribute(r, "abstractNumId");
                QHash<int, NumLevel> levels;
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("lvl")) {
                        const int ilvl = attribute(r, "ilvl").toInt();
                        NumLevel level;
                        while (r.readNextStartElement()) {
                            if (r.name() == QLatin1String("start"))
                                level.start = attribute(r, "val").toInt();
                            else if (r.name() == QLatin1String("numFmt"))
                                level.format = attribute(r, "val");
                            else if (r.name() == QLatin1String("lvlText"))
                                level.text = attribute(r, "val");
                            r.skipCurrentElement();
                        }
                        levels.insert(ilvl, level);
                    } else {
                        r.skipCurrentElement();
                    }
                }
                m_abstractNums.insert(id, levels);
            } else if (r.name() == QLatin1String("num")) {
                const QString numId = attribute(r, "numId");
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("abstractNumId")) {
                        m_numToAbstract.insert(numId, attribute(r, "val"));
                        r.skipCurrentElement();
                    } else if (r.name() == QLatin1String("lvlOverride")) {
                        const int ilvl = attribute(r, "ilvl").toInt();
                        while (r.readNextStartElement()) {
                            if (r.name() == QLatin1String("startOverride"))
                                m_startOverrides[numId].insert(ilvl, attribute(r, "val").toInt());
                            r.skipCurrentElement();
                        }
                    } else {
                        r.skipCurrentElement();
                    }
                }
            } else {
                r.skipCurrentElement();
            }
        }
    }

    void readCoreProperties(DocxMetadata *meta) const
    {
        const QByteArray data = m_zip->read(QStringLiteral("docProps/core.xml"));
        QXmlStreamReader r(data);
        while (!r.atEnd()) {
            if (r.readNext() != QXmlStreamReader::StartElement)
                continue;
            if (r.name() == QLatin1String("title"))
                meta->title = r.readElementText();
            else if (r.name() == QLatin1String("creator"))
                meta->author = r.readElementText();
        }
    }

    // ---- styles -----------------------------------------------------------------

    QTextBlockFormat styleBlockFormat(const QString &id, int depth = 0) const
    {
        const auto it = m_styles.constFind(id);
        if (it == m_styles.cend() || depth > 12)
            return {};
        QTextBlockFormat f = it->basedOn.isEmpty() ? QTextBlockFormat() : styleBlockFormat(it->basedOn, depth + 1);
        f.merge(it->bf);
        return f;
    }

    QTextCharFormat styleCharFormat(const QString &id, int depth = 0) const
    {
        const auto it = m_styles.constFind(id);
        if (it == m_styles.cend() || depth > 12)
            return {};
        QTextCharFormat f = it->basedOn.isEmpty() ? QTextCharFormat() : styleCharFormat(it->basedOn, depth + 1);
        f.merge(it->cf);
        return f;
    }

    int headingLevel(const QString &id, int depth = 0) const
    {
        const auto it = m_styles.constFind(id);
        if (it == m_styles.cend() || depth > 12)
            return 0;
        if (it->outlineLevel > 0)
            return it->outlineLevel;
        static const QRegularExpression re(QStringLiteral("^heading\\s*([1-9])$"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(it->name);
        if (m.hasMatch())
            return m.captured(1).toInt();
        return it->basedOn.isEmpty() ? 0 : headingLevel(it->basedOn, depth + 1);
    }

    QString knownStyleId(const QString &id, int level) const
    {
        if (level >= 1 && level <= 3)
            return QStringLiteral("Heading%1").arg(level);
        const QString name = m_styles.value(id).name.toLower();
        if (id == QLatin1String("Title") || name == QLatin1String("title"))
            return QStringLiteral("Title");
        if (id == QLatin1String("Subtitle") || name == QLatin1String("subtitle"))
            return QStringLiteral("Subtitle");
        if (id == QLatin1String("Quote") || name == QLatin1String("quote"))
            return QStringLiteral("Quote");
        if (id == QLatin1String("Code"))
            return QStringLiteral("Code");
        return {};
    }

    QTextListFormat listFormat(const QString &numId, int ilvl) const
    {
        const QHash<int, NumLevel> levels = m_abstractNums.value(m_numToAbstract.value(numId));
        const NumLevel level = levels.value(ilvl);
        QTextListFormat lf;
        lf.setIndent(ilvl + 1);
        const QString fmt = level.format;
        if (fmt == QLatin1String("bullet") || fmt.isEmpty()) {
            const QString t = level.text;
            if (t.contains(QLatin1Char('o')) || t.contains(QChar(0x25E6)) || t.contains(QChar(0x25CB)))
                lf.setStyle(QTextListFormat::ListCircle);
            else if (t.contains(QChar(0x25AA)) || t.contains(QChar(0x25A0)) || t.contains(QChar(0xF0A7)))
                lf.setStyle(QTextListFormat::ListSquare);
            else
                lf.setStyle(QTextListFormat::ListDisc);
            return lf;
        }
        if (fmt == QLatin1String("lowerLetter"))
            lf.setStyle(QTextListFormat::ListLowerAlpha);
        else if (fmt == QLatin1String("upperLetter"))
            lf.setStyle(QTextListFormat::ListUpperAlpha);
        else if (fmt == QLatin1String("lowerRoman"))
            lf.setStyle(QTextListFormat::ListLowerRoman);
        else if (fmt == QLatin1String("upperRoman"))
            lf.setStyle(QTextListFormat::ListUpperRoman);
        else
            lf.setStyle(QTextListFormat::ListDecimal);
        const QString t = level.text;
        const int percent = t.indexOf(QLatin1Char('%'));
        if (percent >= 0) {
            lf.setNumberPrefix(t.left(percent));
            lf.setNumberSuffix(t.mid(percent + 2));
        }
        const int start = m_startOverrides.value(numId).value(ilvl, level.start);
        lf.setStart(qMax(1, start));
        return lf;
    }

    // ---- property parsing --------------------------------------------------------

    void parseRunProperties(QXmlStreamReader &r, QTextCharFormat &cf, QString *runStyle)
    {
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("rStyle")) {
                if (runStyle)
                    *runStyle = attribute(r, "val");
            } else if (n == QLatin1String("rFonts")) {
                QString face = attribute(r, "ascii");
                if (face.isEmpty())
                    face = attribute(r, "hAnsi");
                if (face.isEmpty()) {
                    const QString theme = attribute(r, "asciiTheme");
                    if (theme.startsWith(QLatin1String("major")))
                        face = m_majorFont.isEmpty() ? QStringLiteral("Calibri Light") : m_majorFont;
                    else if (theme.startsWith(QLatin1String("minor")))
                        face = m_minorFont.isEmpty() ? QStringLiteral("Calibri") : m_minorFont;
                }
                if (face.isEmpty())
                    face = attribute(r, "cs");
                if (!face.isEmpty())
                    cf.setFontFamilies(QStringList{face});
            } else if (n == QLatin1String("b")) {
                cf.setFontWeight(Ooxml::isOn(r) ? QFont::Bold : QFont::Normal);
            } else if (n == QLatin1String("i")) {
                cf.setFontItalic(Ooxml::isOn(r));
            } else if (n == QLatin1String("u")) {
                const QString v = attribute(r, "val");
                QTextCharFormat::UnderlineStyle style = QTextCharFormat::SingleUnderline;
                if (v == QLatin1String("none"))
                    style = QTextCharFormat::NoUnderline;
                else if (v == QLatin1String("dotted"))
                    style = QTextCharFormat::DotLine;
                else if (v.startsWith(QLatin1String("dash")))
                    style = QTextCharFormat::DashUnderline;
                else if (v == QLatin1String("dotDash"))
                    style = QTextCharFormat::DashDotLine;
                else if (v == QLatin1String("dotDotDash"))
                    style = QTextCharFormat::DashDotDotLine;
                else if (v.startsWith(QLatin1String("wav")))
                    style = QTextCharFormat::WaveUnderline;
                cf.setUnderlineStyle(style);
            } else if (n == QLatin1String("strike") || n == QLatin1String("dstrike")) {
                cf.setFontStrikeOut(Ooxml::isOn(r));
            } else if (n == QLatin1String("caps")) {
                cf.setFontCapitalization(Ooxml::isOn(r) ? QFont::AllUppercase : QFont::MixedCase);
            } else if (n == QLatin1String("smallCaps")) {
                cf.setFontCapitalization(Ooxml::isOn(r) ? QFont::SmallCaps : QFont::MixedCase);
            } else if (n == QLatin1String("color")) {
                const QColor c = hexColor(attribute(r, "val"));
                if (c.isValid())
                    cf.setForeground(c);
            } else if (n == QLatin1String("sz")) {
                const double half = attribute(r, "val").toDouble();
                if (half > 0)
                    cf.setFontPointSize(half / 2.0);
            } else if (n == QLatin1String("highlight")) {
                const QColor c = highlightColor(attribute(r, "val"));
                if (c.isValid())
                    cf.setBackground(c);
            } else if (n == QLatin1String("shd")) {
                const QColor c = hexColor(attribute(r, "fill"));
                if (c.isValid())
                    cf.setBackground(c);
            } else if (n == QLatin1String("vertAlign")) {
                const QString v = attribute(r, "val");
                if (v == QLatin1String("superscript"))
                    cf.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
                else if (v == QLatin1String("subscript"))
                    cf.setVerticalAlignment(QTextCharFormat::AlignSubScript);
                else
                    cf.setVerticalAlignment(QTextCharFormat::AlignNormal);
            }
            r.skipCurrentElement();
        }
    }

    void parseParagraphProperties(QXmlStreamReader &r, ParaProps &p)
    {
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("pStyle")) {
                p.styleId = attribute(r, "val");
                r.skipCurrentElement();
            } else if (n == QLatin1String("numPr")) {
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("ilvl"))
                        p.ilvl = attribute(r, "val").toInt();
                    else if (r.name() == QLatin1String("numId"))
                        p.numId = attribute(r, "val");
                    r.skipCurrentElement();
                }
            } else if (n == QLatin1String("jc")) {
                const QString v = attribute(r, "val");
                if (v == QLatin1String("center"))
                    p.bf.setAlignment(Qt::AlignHCenter);
                else if (v == QLatin1String("right") || v == QLatin1String("end"))
                    p.bf.setAlignment(Qt::AlignRight);
                else if (v == QLatin1String("both") || v == QLatin1String("distribute"))
                    p.bf.setAlignment(Qt::AlignJustify);
                else
                    p.bf.setAlignment(Qt::AlignLeft);
                r.skipCurrentElement();
            } else if (n == QLatin1String("spacing")) {
                const QString before = attribute(r, "before");
                const QString after = attribute(r, "after");
                const QString line = attribute(r, "line");
                const QString rule = attribute(r, "lineRule");
                if (!before.isEmpty())
                    p.bf.setTopMargin(twipsToPx(before));
                if (!after.isEmpty())
                    p.bf.setBottomMargin(twipsToPx(after));
                if (!line.isEmpty()) {
                    if (rule == QLatin1String("exact"))
                        p.bf.setLineHeight(twipsToPx(line), QTextBlockFormat::FixedHeight);
                    else if (rule == QLatin1String("atLeast"))
                        p.bf.setLineHeight(twipsToPx(line), QTextBlockFormat::MinimumHeight);
                    else
                        p.bf.setLineHeight(line.toDouble() / 2.4, QTextBlockFormat::ProportionalHeight);
                }
                r.skipCurrentElement();
            } else if (n == QLatin1String("ind")) {
                QString left = attribute(r, "left");
                if (left.isEmpty())
                    left = attribute(r, "start");
                QString right = attribute(r, "right");
                if (right.isEmpty())
                    right = attribute(r, "end");
                if (!left.isEmpty())
                    p.bf.setLeftMargin(twipsToPx(left));
                if (!right.isEmpty())
                    p.bf.setRightMargin(twipsToPx(right));
                const QString first = attribute(r, "firstLine");
                const QString hanging = attribute(r, "hanging");
                if (!hanging.isEmpty())
                    p.bf.setTextIndent(-twipsToPx(hanging));
                else if (!first.isEmpty())
                    p.bf.setTextIndent(twipsToPx(first));
                r.skipCurrentElement();
            } else if (n == QLatin1String("pageBreakBefore")) {
                p.pageBreakBefore = Ooxml::isOn(r);
                r.skipCurrentElement();
            } else if (n == QLatin1String("shd")) {
                const QColor c = hexColor(attribute(r, "fill"));
                if (c.isValid())
                    p.bf.setBackground(c);
                r.skipCurrentElement();
            } else if (n == QLatin1String("pBdr")) {
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("bottom")) {
                        const QString v = attribute(r, "val");
                        if (!v.isEmpty() && v != QLatin1String("none") && v != QLatin1String("nil"))
                            p.bf.setProperty(QTextFormat::BlockTrailingHorizontalRulerWidth,
                                             QTextLength(QTextLength::PercentageLength, 100));
                    }
                    r.skipCurrentElement();
                }
            } else if (n == QLatin1String("outlineLvl")) {
                const int level = attribute(r, "val").toInt();
                if (level >= 0 && level < 9)
                    p.outlineLevel = level + 1;
                r.skipCurrentElement();
            } else if (n == QLatin1String("tabs")) {
                QList<QTextOption::Tab> tabs;
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("tab")) {
                        const QString v = attribute(r, "val");
                        if (v != QLatin1String("clear")) {
                            QTextOption::TabType type = QTextOption::LeftTab;
                            if (v == QLatin1String("right") || v == QLatin1String("end"))
                                type = QTextOption::RightTab;
                            else if (v == QLatin1String("center"))
                                type = QTextOption::CenterTab;
                            tabs << QTextOption::Tab(twipsToPx(attribute(r, "pos")), type);
                        }
                    }
                    r.skipCurrentElement();
                }
                if (!tabs.isEmpty())
                    p.bf.setTabPositions(tabs);
            } else if (n == QLatin1String("rPr")) {
                parseRunProperties(r, p.markCf, nullptr);
            } else if (n == QLatin1String("sectPr")) {
                parseSectionProperties(r);
            } else {
                r.skipCurrentElement();
            }
        }
    }

    void parseSectionProperties(QXmlStreamReader &r)
    {
        double w = 0;
        double h = 0;
        bool landscape = false;
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("pgSz")) {
                w = attribute(r, "w").toDouble();
                h = attribute(r, "h").toDouble();
                landscape = attribute(r, "orient") == QLatin1String("landscape");
            } else if (n == QLatin1String("pgMar")) {
                auto mm = [&](const char *name) { return attribute(r, name).toDouble() / 1440.0 * 25.4; };
                m_page.margins = QMarginsF(mm("left"), mm("top"), mm("right"), mm("bottom"));
            } else if (n == QLatin1String("headerReference")) {
                if (attribute(r, "type") == QLatin1String("default"))
                    m_headerRid = attribute(r, "id");
            } else if (n == QLatin1String("footerReference")) {
                if (attribute(r, "type") == QLatin1String("default"))
                    m_footerRid = attribute(r, "id");
            }
            r.skipCurrentElement();
        }
        if (w > 0 && h > 0) {
            landscape = landscape || w > h;
            const double wmm = w / 1440.0 * 25.4;
            const double hmm = h / 1440.0 * 25.4;
            const QSizeF portrait = landscape ? QSizeF(qMin(wmm, hmm), qMax(wmm, hmm)) : QSizeF(wmm, hmm);
            m_page.pageSize = QPageSize(portrait, QPageSize::Millimeter, QString(), QPageSize::FuzzyMatch);
            m_page.orientation = landscape ? QPageLayout::Landscape : QPageLayout::Portrait;
        }
    }

    // ---- document content --------------------------------------------------------

    void parseBlockContent(QXmlStreamReader &r)
    {
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("p"))
                parseParagraph(r);
            else if (n == QLatin1String("tbl"))
                parseTable(r);
            else if (n == QLatin1String("sdt") || n == QLatin1String("sdtContent") || n == QLatin1String("customXml"))
                parseBlockContent(r);
            else if (n == QLatin1String("sectPr"))
                parseSectionProperties(r);
            else
                r.skipCurrentElement();
        }
    }

    void beginParagraph(const ParaProps &p)
    {
        const QString styleId = p.styleId.isEmpty() ? m_defaultParagraphStyle : p.styleId;
        QTextBlockFormat bf = m_defaultBf;
        bf.merge(styleBlockFormat(styleId));
        bf.merge(p.bf);

        const int level = p.outlineLevel > 0 ? p.outlineLevel : headingLevel(styleId);
        bf.setHeadingLevel(qBound(0, level, 6));
        const QString known = knownStyleId(styleId, level);
        if (!known.isEmpty())
            bf.setProperty(Docx::StyleIdProperty, known);
        if (m_pendingPageBreak || p.pageBreakBefore) {
            bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
            m_pendingPageBreak = false;
        }

        QString numId = p.numId;
        int ilvl = p.ilvl;
        if (numId.isEmpty()) {
            numId = m_styles.value(styleId).numId;
            if (ilvl < 0)
                ilvl = m_styles.value(styleId).ilvl;
        }
        const bool inList = !numId.isEmpty() && numId != QLatin1String("0") && m_numToAbstract.contains(numId);
        if (inList) {
            bf.setLeftMargin(0);
            bf.setTextIndent(0);
            bf.setIndent(0);
        }

        m_paragraphCf = m_defaultCf;
        m_paragraphCf.merge(styleCharFormat(styleId));
        QTextCharFormat markCf = m_paragraphCf;
        markCf.merge(p.markCf);

        if (m_reuseBlock) {
            m_cursor.setBlockFormat(bf);
            m_cursor.setBlockCharFormat(markCf);
            m_reuseBlock = false;
        } else {
            m_cursor.insertBlock(bf, markCf);
        }
        m_inParagraph = true;

        if (inList) {
            ilvl = qBound(0, ilvl, 8);
            const QString key = numId + QLatin1Char('|') + QString::number(ilvl);
            if (QTextList *list = m_lists.value(key))
                list->add(m_cursor.block());
            else
                m_lists.insert(key, m_cursor.createList(listFormat(numId, ilvl)));
        }
    }

    void parseParagraph(QXmlStreamReader &r)
    {
        ParaProps props;
        bool started = false;
        auto start = [&] {
            if (!started) {
                beginParagraph(props);
                started = true;
            }
        };
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("pPr")) {
                parseParagraphProperties(r, props);
            } else if (n == QLatin1String("r")) {
                start();
                parseRun(r, QString());
            } else if (n == QLatin1String("hyperlink")) {
                start();
                parseHyperlink(r);
            } else if (n == QLatin1String("bookmarkStart")) {
                start();
                const QString name = attribute(r, "name");
                if (!name.isEmpty() && !name.startsWith(QLatin1String("_GoBack")))
                    m_pendingAnchors << name;
                r.skipCurrentElement();
            } else if (n == QLatin1String("ins") || n == QLatin1String("smartTag") || n == QLatin1String("customXml")
                       || n == QLatin1String("fldSimple") || n == QLatin1String("sdt") || n == QLatin1String("sdtContent")
                       || n == QLatin1String("dir") || n == QLatin1String("bdo")) {
                start();
                parseRunContainer(r, QString());
            } else {
                r.skipCurrentElement();
            }
        }
        start();
        m_inParagraph = false;
    }

    void parseRunContainer(QXmlStreamReader &r, const QString &href)
    {
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("r"))
                parseRun(r, href);
            else if (n == QLatin1String("hyperlink"))
                parseHyperlink(r);
            else if (n == QLatin1String("del") || n == QLatin1String("sdtPr") || n == QLatin1String("rPr"))
                r.skipCurrentElement();
            else
                parseRunContainer(r, href);
        }
    }

    void parseHyperlink(QXmlStreamReader &r)
    {
        QString href;
        const QString rid = attribute(r, "id");
        const QString anchor = attribute(r, "anchor");
        if (!rid.isEmpty())
            href = m_rels.value(rid).target;
        else if (!anchor.isEmpty())
            href = QLatin1Char('#') + anchor;
        parseRunContainer(r, href);
    }

    bool inFieldInstruction() const { return !m_fieldState.isEmpty() && m_fieldState.last() == 0; }

    // A page break followed by more content of the same paragraph splits the paragraph.
    void flushPendingPageBreak()
    {
        if (!m_pendingPageBreak || !m_inParagraph)
            return;
        QTextBlockFormat bf = m_cursor.blockFormat();
        bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        m_cursor.insertBlock(bf, m_cursor.blockCharFormat());
        m_pendingPageBreak = false;
    }

    void insertText(const QString &text, QTextCharFormat format, const QString &href)
    {
        if (text.isEmpty() || inFieldInstruction())
            return;
        flushPendingPageBreak();
        if (!href.isEmpty()) {
            format.setAnchor(true);
            format.setAnchorHref(href);
            if (!format.hasProperty(QTextFormat::ForegroundBrush))
                format.setForeground(QColor(0x05, 0x63, 0xC1));
            if (!format.hasProperty(QTextFormat::TextUnderlineStyle))
                format.setFontUnderline(true);
        }
        if (!m_pendingAnchors.isEmpty()) {
            format.setAnchor(true);
            format.setAnchorNames(m_pendingAnchors);
            m_pendingAnchors.clear();
        }
        m_cursor.insertText(text, format);
    }

    void parseRun(QXmlStreamReader &r, const QString &href)
    {
        QTextCharFormat cf = m_paragraphCf;
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("rPr")) {
                QTextCharFormat direct;
                QString runStyle;
                parseRunProperties(r, direct, &runStyle);
                cf = m_paragraphCf;
                if (!runStyle.isEmpty())
                    cf.merge(styleCharFormat(runStyle));
                cf.merge(direct);
            } else if (n == QLatin1String("t")) {
                insertText(r.readElementText(), cf, href);
            } else if (n == QLatin1String("tab")) {
                insertText(QStringLiteral("\t"), cf, href);
                r.skipCurrentElement();
            } else if (n == QLatin1String("br")) {
                if (attribute(r, "type") == QLatin1String("page"))
                    m_pendingPageBreak = true;
                else
                    insertText(QString(QChar::LineSeparator), cf, href);
                r.skipCurrentElement();
            } else if (n == QLatin1String("cr")) {
                insertText(QString(QChar::LineSeparator), cf, href);
                r.skipCurrentElement();
            } else if (n == QLatin1String("noBreakHyphen")) {
                insertText(QStringLiteral("-"), cf, href);
                r.skipCurrentElement();
            } else if (n == QLatin1String("fldChar")) {
                const QString type = attribute(r, "fldCharType");
                if (type == QLatin1String("begin"))
                    m_fieldState << 0;
                else if (type == QLatin1String("separate") && !m_fieldState.isEmpty())
                    m_fieldState.last() = 1;
                else if (type == QLatin1String("end") && !m_fieldState.isEmpty())
                    m_fieldState.removeLast();
                r.skipCurrentElement();
            } else if (n == QLatin1String("drawing") || n == QLatin1String("pict") || n == QLatin1String("object")
                       || n == QLatin1String("AlternateContent")) {
                parseGraphic(r, cf);
            } else {
                r.skipCurrentElement();
            }
        }
    }

    // Finds an embedded picture inside a drawing / VML / AlternateContent subtree.
    void parseGraphic(QXmlStreamReader &r, const QTextCharFormat &cf)
    {
        qint64 cx = 0;
        qint64 cy = 0;
        QString embed;
        QString vmlStyle;
        int depth = 1;
        int fallbackDepth = 0;
        while (!r.atEnd() && depth > 0) {
            const QXmlStreamReader::TokenType t = r.readNext();
            if (t == QXmlStreamReader::StartElement) {
                ++depth;
                const QStringView n = r.name();
                if (n == QLatin1String("Fallback") && fallbackDepth == 0)
                    fallbackDepth = depth;
                if (fallbackDepth > 0)
                    continue;
                if (n == QLatin1String("extent") && cx == 0) {
                    cx = attribute(r, "cx").toLongLong();
                    cy = attribute(r, "cy").toLongLong();
                } else if (n == QLatin1String("blip") && embed.isEmpty()) {
                    embed = attribute(r, "embed");
                } else if (n == QLatin1String("shape")) {
                    vmlStyle = attribute(r, "style");
                } else if (n == QLatin1String("imagedata") && embed.isEmpty()) {
                    embed = attribute(r, "id");
                }
            } else if (t == QXmlStreamReader::EndElement) {
                if (depth == fallbackDepth)
                    fallbackDepth = 0;
                --depth;
            }
        }
        if (embed.isEmpty())
            return;
        qreal wpx = cx / 9525.0;
        qreal hpx = cy / 9525.0;
        if (wpx <= 0 && !vmlStyle.isEmpty()) {
            static const QRegularExpression re(QStringLiteral("(width|height)\\s*:\\s*([0-9.]+)(pt|px|in)?"));
            QRegularExpressionMatchIterator it = re.globalMatch(vmlStyle);
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                double v = m.captured(2).toDouble();
                if (m.captured(3) == QLatin1String("pt") || m.captured(3).isEmpty())
                    v = v * 96.0 / 72.0;
                else if (m.captured(3) == QLatin1String("in"))
                    v *= 96.0;
                (m.captured(1) == QLatin1String("width") ? wpx : hpx) = v;
            }
        }
        insertImage(embed, wpx, hpx, cf);
    }

    void insertImage(const QString &rid, qreal wpx, qreal hpx, const QTextCharFormat &cf)
    {
        const Ooxml::Relationship rel = m_rels.value(rid);
        if (rel.external || rel.target.isEmpty())
            return;
        QImage image;
        if (!image.loadFromData(m_zip->read(rel.target)))
            return;
        const QString name = QStringLiteral("docx-image-%1.png").arg(QUuid::createUuid().toString(QUuid::Id128));
        m_doc->addResource(QTextDocument::ImageResource, QUrl(name), image);
        QTextImageFormat f;
        f.merge(cf);
        f.setName(name);
        f.setWidth(wpx > 0 ? wpx : image.width());
        f.setHeight(hpx > 0 ? hpx : image.height());
        flushPendingPageBreak();
        m_cursor.insertImage(f);
    }

    void parseTable(QXmlStreamReader &r)
    {
        QTextTableFormat tf;
        tf.setCellSpacing(0);
        tf.setCellPadding(4);
        tf.setBorderCollapse(true);
        tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
        tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        tf.setBorderBrush(QColor(0x8C, 0x8C, 0x8C));
        bool borders = false;
        bool explicitNoBorders = false;
        QList<int> gridCols;
        QTextTable *table = nullptr;
        int row = -1;
        QHash<int, int> vMergeStart;

        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("tblPr")) {
                while (r.readNextStartElement()) {
                    const QStringView pn = r.name();
                    if (pn == QLatin1String("tblStyle")) {
                        const QString style = attribute(r, "val");
                        if (style.contains(QLatin1String("Grid"), Qt::CaseInsensitive)
                            || m_styles.value(style).name.contains(QLatin1String("Grid"), Qt::CaseInsensitive))
                            borders = true;
                        r.skipCurrentElement();
                    } else if (pn == QLatin1String("tblW")) {
                        const QString type = attribute(r, "type");
                        const double w = attribute(r, "w").toDouble();
                        if (type == QLatin1String("pct") && w > 0)
                            tf.setWidth(QTextLength(QTextLength::PercentageLength, qMin(100.0, w / 50.0)));
                        else if (type == QLatin1String("dxa") && w > 0)
                            tf.setWidth(QTextLength(QTextLength::FixedLength, w / 15.0));
                        r.skipCurrentElement();
                    } else if (pn == QLatin1String("jc")) {
                        if (attribute(r, "val") == QLatin1String("center"))
                            tf.setAlignment(Qt::AlignHCenter);
                        r.skipCurrentElement();
                    } else if (pn == QLatin1String("tblBorders")) {
                        bool any = false;
                        while (r.readNextStartElement()) {
                            const QString v = attribute(r, "val");
                            if (!v.isEmpty() && v != QLatin1String("none") && v != QLatin1String("nil"))
                                any = true;
                            r.skipCurrentElement();
                        }
                        borders = any;
                        explicitNoBorders = !any;
                    } else if (pn == QLatin1String("tblCellMar")) {
                        while (r.readNextStartElement()) {
                            if (r.name() == QLatin1String("left") || r.name() == QLatin1String("start"))
                                tf.setCellPadding(qMax(2.0, attribute(r, "w").toDouble() / 15.0));
                            r.skipCurrentElement();
                        }
                    } else {
                        r.skipCurrentElement();
                    }
                }
            } else if (n == QLatin1String("tblGrid")) {
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("gridCol"))
                        gridCols << qMax(1, attribute(r, "w").toInt());
                    r.skipCurrentElement();
                }
            } else if (n == QLatin1String("tr")) {
                if (!table) {
                    const int cols = qMax(1, int(gridCols.size()));
                    if (!gridCols.isEmpty()) {
                        double total = 0;
                        for (int w : std::as_const(gridCols))
                            total += w;
                        QList<QTextLength> widths;
                        for (int w : std::as_const(gridCols))
                            widths << QTextLength(QTextLength::PercentageLength, w * 100.0 / total);
                        tf.setColumnWidthConstraints(widths);
                    }
                    tf.setBorder(borders && !explicitNoBorders ? 1 : 0);
                    if (m_pendingPageBreak) {
                        QTextBlockFormat bf = m_cursor.blockFormat();
                        bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
                        m_cursor.setBlockFormat(bf);
                        m_pendingPageBreak = false;
                    }
                    table = m_cursor.insertTable(1, cols, tf);
                    row = 0;
                } else {
                    table->appendRows(1);
                    ++row;
                }
                parseRow(r, table, row, vMergeStart);
            } else {
                r.skipCurrentElement();
            }
        }
        if (table) {
            m_cursor.setPosition(table->lastPosition() + 1);
            m_reuseBlock = true;
        }
    }

    void parseRow(QXmlStreamReader &r, QTextTable *table, int row, QHash<int, int> &vMergeStart)
    {
        int col = 0;
        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("trPr")) {
                while (r.readNextStartElement()) {
                    if (r.name() == QLatin1String("tblHeader")) {
                        QTextTableFormat f = table->format();
                        f.setHeaderRowCount(row + 1);
                        table->setFormat(f);
                    }
                    r.skipCurrentElement();
                }
            } else if (n == QLatin1String("tc")) {
                parseCell(r, table, row, col, vMergeStart);
            } else {
                r.skipCurrentElement();
            }
        }
    }

    void parseCell(QXmlStreamReader &r, QTextTable *table, int row, int &col, QHash<int, int> &vMergeStart)
    {
        int span = 1;
        bool hasVMerge = false;
        bool restart = false;
        QColor shade;
        QString valign;
        bool setupDone = false;
        bool skipContent = false;

        auto setup = [&] {
            setupDone = true;
            if (col + span > table->columns())
                table->appendColumns(col + span - table->columns());
            if (hasVMerge && !restart && vMergeStart.contains(col)) {
                const int startRow = vMergeStart.value(col);
                table->mergeCells(startRow, col, row - startRow + 1, span);
                skipContent = true;
                return;
            }
            if (hasVMerge && restart)
                vMergeStart.insert(col, row);
            else
                vMergeStart.remove(col);
            if (span > 1)
                table->mergeCells(row, col, 1, span);
            QTextTableCell cell = table->cellAt(row, col);
            QTextTableCellFormat cf = cell.format().toTableCellFormat();
            if (shade.isValid())
                cf.setBackground(shade);
            if (valign == QLatin1String("center"))
                cf.setVerticalAlignment(QTextCharFormat::AlignMiddle);
            else if (valign == QLatin1String("bottom"))
                cf.setVerticalAlignment(QTextCharFormat::AlignBottom);
            cell.setFormat(cf);
            m_cursor = cell.firstCursorPosition();
            m_reuseBlock = true;
        };

        while (r.readNextStartElement()) {
            const QStringView n = r.name();
            if (n == QLatin1String("tcPr")) {
                while (r.readNextStartElement()) {
                    const QStringView pn = r.name();
                    if (pn == QLatin1String("gridSpan")) {
                        span = qMax(1, attribute(r, "val").toInt());
                    } else if (pn == QLatin1String("vMerge")) {
                        hasVMerge = true;
                        restart = attribute(r, "val") == QLatin1String("restart");
                    } else if (pn == QLatin1String("shd")) {
                        shade = hexColor(attribute(r, "fill"));
                    } else if (pn == QLatin1String("vAlign")) {
                        valign = attribute(r, "val");
                    }
                    r.skipCurrentElement();
                }
                continue;
            }
            if (!setupDone)
                setup();
            if (skipContent) {
                r.skipCurrentElement();
                continue;
            }
            if (n == QLatin1String("p"))
                parseParagraph(r);
            else if (n == QLatin1String("tbl"))
                parseTable(r);
            else if (n == QLatin1String("sdt") || n == QLatin1String("sdtContent") || n == QLatin1String("customXml"))
                parseBlockContent(r);
            else
                r.skipCurrentElement();
        }
        if (!setupDone)
            setup();
        col += span;
    }

    // ---- post processing ---------------------------------------------------------

    void convertCheckboxes()
    {
        const QChar unchecked(0x2610);
        const QChar checked(0x2612);
        const QChar checkedAlt(0x2611);
        for (QTextBlock block = m_doc->begin(); block.isValid(); block = block.next()) {
            const QString text = block.text();
            if (text.size() < 2 || text.at(1) != QLatin1Char(' '))
                continue;
            const QChar box = text.at(0);
            if (box != unchecked && box != checked && box != checkedAlt)
                continue;
            QTextCursor c(block);
            c.setPosition(block.position() + 2, QTextCursor::KeepAnchor);
            c.removeSelectedText();
            if (!c.currentList()) {
                QTextListFormat lf;
                lf.setStyle(QTextListFormat::ListDisc);
                lf.setIndent(1);
                c.createList(lf);
            }
            QTextBlockFormat bf;
            bf.setMarker(box == unchecked ? QTextBlockFormat::MarkerType::Unchecked : QTextBlockFormat::MarkerType::Checked);
            c.mergeBlockFormat(bf);
        }
    }

    QString headerFooterText(const QString &rid) const
    {
        if (rid.isEmpty())
            return {};
        const QByteArray data = m_zip->read(m_rels.value(rid).target);
        if (data.isEmpty())
            return {};
        auto token = [](const QString &instr) -> QString {
            const QString word = instr.trimmed().section(QLatin1Char(' '), 0, 0).toUpper();
            if (word == QLatin1String("PAGE"))
                return QStringLiteral("{page}");
            if (word == QLatin1String("NUMPAGES") || word == QLatin1String("SECTIONPAGES"))
                return QStringLiteral("{pages}");
            if (word == QLatin1String("DATE"))
                return QStringLiteral("{date}");
            return {};
        };
        QXmlStreamReader r(data);
        QString text;
        QString instr;
        QList<int> state; // 0: instruction, 1: result, 2: result replaced by a token
        bool firstParagraph = true;
        while (!r.atEnd()) {
            if (r.readNext() != QXmlStreamReader::StartElement)
                continue;
            const QStringView n = r.name();
            if (n == QLatin1String("p")) {
                if (!firstParagraph && !text.isEmpty())
                    text += QLatin1Char(' ');
                firstParagraph = false;
            } else if (n == QLatin1String("t")) {
                const QString s = r.readElementText();
                if (state.isEmpty() || state.last() == 1)
                    text += s;
            } else if (n == QLatin1String("tab")) {
                text += QStringLiteral("  ");
            } else if (n == QLatin1String("fldSimple")) {
                const QString t = token(attribute(r, "instr"));
                if (!t.isEmpty()) {
                    text += t;
                    r.skipCurrentElement();
                }
            } else if (n == QLatin1String("fldChar")) {
                const QString type = attribute(r, "fldCharType");
                if (type == QLatin1String("begin")) {
                    state << 0;
                    instr.clear();
                } else if (type == QLatin1String("separate") && !state.isEmpty()) {
                    const QString t = token(instr);
                    if (!t.isEmpty()) {
                        text += t;
                        state.last() = 2;
                    } else {
                        state.last() = 1;
                    }
                } else if (type == QLatin1String("end") && !state.isEmpty()) {
                    state.removeLast();
                }
            } else if (n == QLatin1String("instrText")) {
                instr += r.readElementText();
            }
        }
        return text.trimmed();
    }

    QTextDocument *m_doc;
    const ZipReader *m_zip = nullptr;
    QString m_docPath;
    QHash<QString, Ooxml::Relationship> m_rels;
    QHash<QString, StyleDef> m_styles;
    QString m_defaultParagraphStyle = QStringLiteral("Normal");
    QString m_majorFont;
    QString m_minorFont;
    QTextCharFormat m_defaultCf;
    QTextBlockFormat m_defaultBf;
    QHash<QString, QHash<int, NumLevel>> m_abstractNums;
    QHash<QString, QString> m_numToAbstract;
    QHash<QString, QHash<int, int>> m_startOverrides;
    QHash<QString, QTextList *> m_lists;

    QTextCursor m_cursor;
    QTextCharFormat m_paragraphCf;
    bool m_reuseBlock = true;
    bool m_inParagraph = false;
    bool m_pendingPageBreak = false;
    QStringList m_pendingAnchors;
    QList<int> m_fieldState;

    PageSetup m_page;
    QString m_headerRid;
    QString m_footerRid;
};

} // namespace

namespace Docx {

bool readData(const QByteArray &data, QTextDocument *document, PageSetup *page, DocxMetadata *meta,
              QString *errorMessage)
{
    ZipReader zip;
    if (!zip.openData(data, errorMessage))
        return false;
    DocxParser parser(document);
    return parser.parse(zip, page, meta, errorMessage);
}

bool read(const QString &path, QTextDocument *document, PageSetup *page, DocxMetadata *meta, QString *errorMessage)
{
    ZipReader zip;
    if (!zip.open(path, errorMessage))
        return false;
    DocxParser parser(document);
    return parser.parse(zip, page, meta, errorMessage);
}

} // namespace Docx
