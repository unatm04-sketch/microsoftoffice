#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

class QXmlStreamReader;
class ZipReader;

// Helpers shared by the Office Open XML readers and writers.
namespace Ooxml {

inline constexpr char kRelationshipsNs[] = "http://schemas.openxmlformats.org/package/2006/relationships";
inline constexpr char kOfficeDocumentRel[] = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument";
inline constexpr char kRelBase[] = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";

struct Relationship
{
    QString id;
    QString type;
    QString target;
    bool external = false;
};

// Builds a .rels part.
class RelationshipWriter
{
public:
    QString add(const QString &type, const QString &target, bool external = false);
    void addWithId(const QString &id, const QString &type, const QString &target, bool external = false);
    QByteArray toXml() const;
    bool isEmpty() const { return m_rels.isEmpty(); }

private:
    QList<Relationship> m_rels;
    int m_next = 1;
};

QString relsPathFor(const QString &partPath);
QString resolveTarget(const QString &partPath, const QString &target);
QHash<QString, Relationship> readRelationships(const ZipReader &zip, const QString &partPath);
// Main part of the package (target of the officeDocument relationship).
QString mainPartPath(const ZipReader &zip, const QString &fallback);

// Attribute lookup by local name, ignoring namespace prefixes.
QString attribute(const QXmlStreamReader &reader, const char *localName);
// OOXML boolean attribute (missing attribute counts as true for toggle elements).
bool isOn(const QXmlStreamReader &reader);

QByteArray corePropertiesXml(const QString &title, const QString &author);
QByteArray appPropertiesXml();

} // namespace Ooxml
