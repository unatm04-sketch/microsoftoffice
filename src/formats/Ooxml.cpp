#include "formats/Ooxml.h"

#include "formats/Zip.h"

#include <QDateTime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace Ooxml {

QString RelationshipWriter::add(const QString &type, const QString &target, bool external)
{
    const QString id = QStringLiteral("rId%1").arg(m_next++);
    m_rels << Relationship{id, type, target, external};
    return id;
}

void RelationshipWriter::addWithId(const QString &id, const QString &type, const QString &target, bool external)
{
    m_rels << Relationship{id, type, target, external};
}

QByteArray RelationshipWriter::toXml() const
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("Relationships"));
    w.writeDefaultNamespace(QString::fromLatin1(kRelationshipsNs));
    for (const Relationship &r : m_rels) {
        w.writeStartElement(QStringLiteral("Relationship"));
        w.writeAttribute(QStringLiteral("Id"), r.id);
        w.writeAttribute(QStringLiteral("Type"), r.type);
        w.writeAttribute(QStringLiteral("Target"), r.target);
        if (r.external)
            w.writeAttribute(QStringLiteral("TargetMode"), QStringLiteral("External"));
        w.writeEndElement();
    }
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

QString relsPathFor(const QString &partPath)
{
    const int slash = partPath.lastIndexOf(QLatin1Char('/'));
    const QString dir = slash >= 0 ? partPath.left(slash + 1) : QString();
    const QString file = partPath.mid(slash + 1);
    return dir + QStringLiteral("_rels/") + file + QStringLiteral(".rels");
}

QString resolveTarget(const QString &partPath, const QString &target)
{
    if (target.startsWith(QLatin1Char('/')))
        return target.mid(1);
    const int slash = partPath.lastIndexOf(QLatin1Char('/'));
    QStringList parts = slash >= 0 ? partPath.left(slash).split(QLatin1Char('/'), Qt::SkipEmptyParts) : QStringList();
    const QStringList targetParts = target.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &p : targetParts) {
        if (p == QLatin1String(".."))
            parts.removeLast();
        else if (p != QLatin1String("."))
            parts << p;
    }
    return parts.join(QLatin1Char('/'));
}

QHash<QString, Relationship> readRelationships(const ZipReader &zip, const QString &partPath)
{
    QHash<QString, Relationship> rels;
    const QByteArray data = zip.read(relsPathFor(partPath));
    if (data.isEmpty())
        return rels;
    QXmlStreamReader r(data);
    while (!r.atEnd()) {
        if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("Relationship")) {
            Relationship rel;
            rel.id = attribute(r, "Id");
            rel.type = attribute(r, "Type");
            rel.external = attribute(r, "TargetMode").compare(QLatin1String("External"), Qt::CaseInsensitive) == 0;
            const QString target = attribute(r, "Target");
            rel.target = rel.external ? target : resolveTarget(partPath, target);
            rels.insert(rel.id, rel);
        }
    }
    return rels;
}

QString mainPartPath(const ZipReader &zip, const QString &fallback)
{
    const QByteArray data = zip.read(QStringLiteral("_rels/.rels"));
    QXmlStreamReader r(data);
    while (!r.atEnd()) {
        if (r.readNext() == QXmlStreamReader::StartElement && r.name() == QLatin1String("Relationship")
            && attribute(r, "Type").endsWith(QLatin1String("/officeDocument"))) {
            QString target = attribute(r, "Target");
            if (target.startsWith(QLatin1Char('/')))
                target = target.mid(1);
            if (zip.contains(target))
                return target;
        }
    }
    return fallback;
}

QString attribute(const QXmlStreamReader &reader, const char *localName)
{
    const QXmlStreamAttributes attrs = reader.attributes();
    for (const QXmlStreamAttribute &a : attrs) {
        if (a.name() == QLatin1String(localName))
            return a.value().toString();
    }
    return {};
}

bool isOn(const QXmlStreamReader &reader)
{
    const QString v = attribute(reader, "val").toLower();
    return !(v == QLatin1String("0") || v == QLatin1String("false") || v == QLatin1String("off")
             || v == QLatin1String("none"));
}

QByteArray corePropertiesXml(const QString &title, const QString &author)
{
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(QStringLiteral("cp:coreProperties"));
    w.writeAttribute(QStringLiteral("xmlns:cp"), QStringLiteral("http://schemas.openxmlformats.org/package/2006/metadata/core-properties"));
    w.writeAttribute(QStringLiteral("xmlns:dc"), QStringLiteral("http://purl.org/dc/elements/1.1/"));
    w.writeAttribute(QStringLiteral("xmlns:dcterms"), QStringLiteral("http://purl.org/dc/terms/"));
    w.writeAttribute(QStringLiteral("xmlns:dcmitype"), QStringLiteral("http://purl.org/dc/dcmitype/"));
    w.writeAttribute(QStringLiteral("xmlns:xsi"), QStringLiteral("http://www.w3.org/2001/XMLSchema-instance"));
    if (!title.isEmpty())
        w.writeTextElement(QStringLiteral("dc:title"), title);
    if (!author.isEmpty()) {
        w.writeTextElement(QStringLiteral("dc:creator"), author);
        w.writeTextElement(QStringLiteral("cp:lastModifiedBy"), author);
    }
    w.writeStartElement(QStringLiteral("dcterms:created"));
    w.writeAttribute(QStringLiteral("xsi:type"), QStringLiteral("dcterms:W3CDTF"));
    w.writeCharacters(now);
    w.writeEndElement();
    w.writeStartElement(QStringLiteral("dcterms:modified"));
    w.writeAttribute(QStringLiteral("xsi:type"), QStringLiteral("dcterms:W3CDTF"));
    w.writeCharacters(now);
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();
    return out;
}

QByteArray appPropertiesXml()
{
    return QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
        "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
        "<Application>OmniOffice</Application><AppVersion>1.0000</AppVersion></Properties>");
}

} // namespace Ooxml
