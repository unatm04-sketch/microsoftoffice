#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// Minimal ZIP container support for the Office Open XML formats.
// Reading handles stored and deflated entries; writing produces deflated entries.
class ZipReader
{
public:
    bool open(const QString &path, QString *errorMessage = nullptr);
    bool openData(const QByteArray &data, QString *errorMessage = nullptr);
    bool isOpen() const { return m_open; }

    QStringList entries() const { return m_order; }
    // Entry names are matched case-insensitively, as OOXML part names are.
    bool contains(const QString &name) const;
    QByteArray read(const QString &name, bool *ok = nullptr) const;

private:
    struct Entry
    {
        quint16 method = 0;
        quint32 compressedSize = 0;
        quint32 size = 0;
        quint32 localOffset = 0;
    };

    QByteArray m_data;
    QHash<QString, Entry> m_entries;
    QStringList m_order;
    bool m_open = false;
};

class ZipWriter
{
public:
    ZipWriter();

    void add(const QString &name, const QByteArray &data, bool compress = true);
    QByteArray finish();
    bool save(const QString &path, QString *errorMessage = nullptr);

private:
    struct Entry
    {
        QByteArray name;
        quint32 crc = 0;
        quint32 compressedSize = 0;
        quint32 size = 0;
        quint16 method = 0;
        quint32 offset = 0;
    };

    QByteArray m_buffer;
    QList<Entry> m_entries;
    quint16 m_dosTime = 0;
    quint16 m_dosDate = 0;
    bool m_finished = false;
};

namespace Zip {
quint32 crc32(const QByteArray &data);
// Decodes a raw DEFLATE stream (RFC 1951).
bool inflate(const QByteArray &compressed, QByteArray *out, qsizetype expectedSize = 0);
// Encodes a raw DEFLATE stream.
QByteArray deflate(const QByteArray &data);
} // namespace Zip
