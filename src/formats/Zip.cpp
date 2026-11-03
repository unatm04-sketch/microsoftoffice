#include "formats/Zip.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QtEndian>

#include <array>

namespace {

// ---------------------------------------------------------------------------------
// CRC-32 (IEEE 802.3)

const std::array<quint32, 256> &crcTable()
{
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> t{};
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    return table;
}

// ---------------------------------------------------------------------------------
// DEFLATE decoder (canonical Huffman decoding as described in RFC 1951).

class Inflater
{
public:
    Inflater(const uchar *data, qsizetype size)
        : m_in(data), m_size(size)
    {
    }

    bool run(QByteArray *out, qsizetype expectedSize)
    {
        if (expectedSize > 0)
            m_out.reserve(expectedSize);
        int last = 0;
        do {
            last = bits(1);
            const int type = bits(2);
            if (m_error)
                return false;
            bool ok = false;
            switch (type) {
            case 0: ok = stored(); break;
            case 1: ok = fixedBlock(); break;
            case 2: ok = dynamicBlock(); break;
            default: ok = false; break;
            }
            if (!ok || m_error)
                return false;
        } while (!last);
        *out = m_out;
        return true;
    }

private:
    static constexpr int kMaxBits = 15;
    static constexpr int kMaxLitCodes = 286;
    static constexpr int kMaxDistCodes = 30;
    static constexpr int kFixedLitCodes = 288;

    struct Huffman
    {
        std::array<short, kMaxBits + 1> count{};
        std::array<short, kFixedLitCodes> symbol{};
    };

    int bits(int need)
    {
        quint32 value = m_bitBuffer;
        while (m_bitCount < need) {
            if (m_pos >= m_size) {
                m_error = true;
                return 0;
            }
            value |= quint32(m_in[m_pos++]) << m_bitCount;
            m_bitCount += 8;
        }
        m_bitBuffer = value >> need;
        m_bitCount -= need;
        return int(value & ((1u << need) - 1u));
    }

    bool stored()
    {
        m_bitBuffer = 0;
        m_bitCount = 0;
        if (m_pos + 4 > m_size)
            return false;
        const unsigned len = m_in[m_pos] | (unsigned(m_in[m_pos + 1]) << 8);
        const unsigned nlen = m_in[m_pos + 2] | (unsigned(m_in[m_pos + 3]) << 8);
        m_pos += 4;
        if (len != (~nlen & 0xFFFFu) || m_pos + qsizetype(len) > m_size)
            return false;
        m_out.append(reinterpret_cast<const char *>(m_in + m_pos), len);
        m_pos += len;
        return true;
    }

    int decode(const Huffman &h)
    {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            code |= bits(1);
            if (m_error)
                return -1;
            const int count = h.count[len];
            if (code - count < first)
                return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }

    static int construct(Huffman &h, const short *length, int n)
    {
        h.count.fill(0);
        for (int symbol = 0; symbol < n; ++symbol)
            ++h.count[length[symbol]];
        if (h.count[0] == n)
            return 0;
        int left = 1;
        for (int len = 1; len <= kMaxBits; ++len) {
            left <<= 1;
            left -= h.count[len];
            if (left < 0)
                return left;
        }
        std::array<short, kMaxBits + 1> offs{};
        offs[1] = 0;
        for (int len = 1; len < kMaxBits; ++len)
            offs[len + 1] = short(offs[len] + h.count[len]);
        for (int symbol = 0; symbol < n; ++symbol) {
            if (length[symbol] != 0)
                h.symbol[offs[length[symbol]]++] = short(symbol);
        }
        return left;
    }

    bool codes(const Huffman &lencode, const Huffman &distcode)
    {
        static const short lens[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                       35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                       3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const short dists[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                        193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                        6145, 8193, 12289, 16385, 24577};
        static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                       7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        int symbol = 0;
        do {
            symbol = decode(lencode);
            if (symbol < 0)
                return false;
            if (symbol < 256) {
                m_out.append(char(symbol));
            } else if (symbol > 256) {
                symbol -= 257;
                if (symbol >= 29)
                    return false;
                const int len = lens[symbol] + bits(lext[symbol]);
                const int distSymbol = decode(distcode);
                if (distSymbol < 0 || distSymbol >= 30)
                    return false;
                const qsizetype dist = dists[distSymbol] + bits(dext[distSymbol]);
                if (m_error || dist > m_out.size())
                    return false;
                const qsizetype from = m_out.size() - dist;
                for (int i = 0; i < len; ++i)
                    m_out.append(m_out.at(from + i));
            }
        } while (symbol != 256);
        return true;
    }

    bool fixedBlock()
    {
        struct Tables
        {
            Huffman lencode;
            Huffman distcode;
        };
        static const Tables tables = [] {
            Tables t;
            short lengths[kFixedLitCodes];
            int symbol = 0;
            for (; symbol < 144; ++symbol)
                lengths[symbol] = 8;
            for (; symbol < 256; ++symbol)
                lengths[symbol] = 9;
            for (; symbol < 280; ++symbol)
                lengths[symbol] = 7;
            for (; symbol < kFixedLitCodes; ++symbol)
                lengths[symbol] = 8;
            construct(t.lencode, lengths, kFixedLitCodes);
            for (symbol = 0; symbol < kMaxDistCodes; ++symbol)
                lengths[symbol] = 5;
            construct(t.distcode, lengths, kMaxDistCodes);
            return t;
        }();
        return codes(tables.lencode, tables.distcode);
    }

    bool dynamicBlock()
    {
        static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        const int nlen = bits(5) + 257;
        const int ndist = bits(5) + 1;
        const int ncode = bits(4) + 4;
        if (m_error || nlen > kMaxLitCodes || ndist > kMaxDistCodes)
            return false;

        short lengths[kMaxLitCodes + kMaxDistCodes] = {};
        int index = 0;
        for (; index < ncode; ++index)
            lengths[order[index]] = short(bits(3));
        for (; index < 19; ++index)
            lengths[order[index]] = 0;

        Huffman lencode;
        Huffman distcode;
        if (construct(lencode, lengths, 19) != 0)
            return false;

        index = 0;
        while (index < nlen + ndist) {
            int symbol = decode(lencode);
            if (symbol < 0)
                return false;
            if (symbol < 16) {
                lengths[index++] = short(symbol);
            } else {
                short len = 0;
                if (symbol == 16) {
                    if (index == 0)
                        return false;
                    len = lengths[index - 1];
                    symbol = 3 + bits(2);
                } else if (symbol == 17) {
                    symbol = 3 + bits(3);
                } else {
                    symbol = 11 + bits(7);
                }
                if (m_error || index + symbol > nlen + ndist)
                    return false;
                while (symbol--)
                    lengths[index++] = len;
            }
        }
        if (lengths[256] == 0)
            return false;

        int err = construct(lencode, lengths, nlen);
        if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1]))
            return false;
        err = construct(distcode, lengths + nlen, ndist);
        if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1]))
            return false;
        return codes(lencode, distcode);
    }

    const uchar *m_in;
    qsizetype m_size;
    qsizetype m_pos = 0;
    quint32 m_bitBuffer = 0;
    int m_bitCount = 0;
    bool m_error = false;
    QByteArray m_out;
};

quint16 readU16(const QByteArray &d, qsizetype pos)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(d.constData()) + pos);
}

quint32 readU32(const QByteArray &d, qsizetype pos)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(d.constData()) + pos);
}

void putU16(QByteArray &d, quint16 v)
{
    uchar b[2];
    qToLittleEndian(v, b);
    d.append(reinterpret_cast<const char *>(b), 2);
}

void putU32(QByteArray &d, quint32 v)
{
    uchar b[4];
    qToLittleEndian(v, b);
    d.append(reinterpret_cast<const char *>(b), 4);
}

QString tr(const char *text)
{
    return QCoreApplication::translate("Zip", text);
}

} // namespace

// ---------------------------------------------------------------------------------

namespace Zip {

quint32 crc32(const QByteArray &data)
{
    const auto &table = crcTable();
    quint32 c = 0xFFFFFFFFu;
    for (const char ch : data)
        c = table[(c ^ uchar(ch)) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool inflate(const QByteArray &compressed, QByteArray *out, qsizetype expectedSize)
{
    Inflater inflater(reinterpret_cast<const uchar *>(compressed.constData()), compressed.size());
    return inflater.run(out, expectedSize);
}

QByteArray deflate(const QByteArray &data)
{
    // qCompress produces [4-byte length][zlib header (2)][raw deflate][adler32 (4)].
    const QByteArray z = qCompress(data, 6);
    if (z.size() <= 10)
        return {};
    return z.mid(6, z.size() - 10);
}

} // namespace Zip

// ---------------------------------------------------------------------------------
// ZipReader

bool ZipReader::open(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = tr("Dosya açılamadı: %1").arg(file.errorString());
        return false;
    }
    return openData(file.readAll(), errorMessage);
}

bool ZipReader::openData(const QByteArray &data, QString *errorMessage)
{
    m_data = data;
    m_entries.clear();
    m_order.clear();
    m_open = false;

    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (m_data.size() < 22)
        return fail(tr("Dosya geçerli bir ZIP/Office belgesi değil."));

    // Locate the end of central directory record (it may be followed by a comment).
    qsizetype eocd = -1;
    const qsizetype minPos = qMax<qsizetype>(0, m_data.size() - 22 - 0xFFFF);
    for (qsizetype pos = m_data.size() - 22; pos >= minPos; --pos) {
        if (readU32(m_data, pos) == 0x06054b50u) {
            eocd = pos;
            break;
        }
    }
    if (eocd < 0)
        return fail(tr("Dosya geçerli bir ZIP/Office belgesi değil."));

    const quint16 total = readU16(m_data, eocd + 10);
    qsizetype pos = readU32(m_data, eocd + 16);
    for (int i = 0; i < total; ++i) {
        if (pos + 46 > m_data.size() || readU32(m_data, pos) != 0x02014b50u)
            return fail(tr("ZIP merkez dizini bozuk."));
        Entry e;
        const quint16 flags = readU16(m_data, pos + 8);
        e.method = readU16(m_data, pos + 10);
        e.compressedSize = readU32(m_data, pos + 20);
        e.size = readU32(m_data, pos + 24);
        const quint16 nameLen = readU16(m_data, pos + 28);
        const quint16 extraLen = readU16(m_data, pos + 30);
        const quint16 commentLen = readU16(m_data, pos + 32);
        e.localOffset = readU32(m_data, pos + 42);
        if (pos + 46 + nameLen > m_data.size())
            return fail(tr("ZIP merkez dizini bozuk."));
        const QByteArray rawName = m_data.mid(pos + 46, nameLen);
        const QString name = (flags & 0x0800) ? QString::fromUtf8(rawName) : QString::fromLatin1(rawName);
        m_entries.insert(name.toLower(), e);
        m_order << name;
        pos += 46 + nameLen + extraLen + commentLen;
    }
    m_open = true;
    return true;
}

bool ZipReader::contains(const QString &name) const
{
    return m_entries.contains(name.toLower());
}

QByteArray ZipReader::read(const QString &name, bool *ok) const
{
    if (ok)
        *ok = false;
    const auto it = m_entries.constFind(name.toLower());
    if (it == m_entries.cend())
        return {};
    const Entry &e = *it;
    const qsizetype local = e.localOffset;
    if (local + 30 > m_data.size() || readU32(m_data, local) != 0x04034b50u)
        return {};
    const qsizetype dataPos = local + 30 + readU16(m_data, local + 26) + readU16(m_data, local + 28);
    if (dataPos + qsizetype(e.compressedSize) > m_data.size())
        return {};
    const QByteArray raw = m_data.mid(dataPos, e.compressedSize);
    QByteArray result;
    if (e.method == 0) {
        result = raw;
    } else if (e.method == 8) {
        if (!Zip::inflate(raw, &result, e.size))
            return {};
    } else {
        return {};
    }
    if (ok)
        *ok = true;
    return result;
}

// ---------------------------------------------------------------------------------
// ZipWriter

ZipWriter::ZipWriter()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDate d = now.date();
    const QTime t = now.time();
    m_dosTime = quint16((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
    m_dosDate = quint16(((qMax(1980, d.year()) - 1980) << 9) | (d.month() << 5) | d.day());
}

void ZipWriter::add(const QString &name, const QByteArray &data, bool compress)
{
    Entry e;
    e.name = name.toUtf8();
    e.crc = Zip::crc32(data);
    e.size = quint32(data.size());
    e.offset = quint32(m_buffer.size());

    QByteArray payload = data;
    e.method = 0;
    if (compress && !data.isEmpty()) {
        const QByteArray deflated = Zip::deflate(data);
        if (!deflated.isEmpty() && deflated.size() < data.size()) {
            payload = deflated;
            e.method = 8;
        }
    }
    e.compressedSize = quint32(payload.size());

    putU32(m_buffer, 0x04034b50u);
    putU16(m_buffer, 20);     // version needed
    putU16(m_buffer, 0x0800); // UTF-8 names
    putU16(m_buffer, e.method);
    putU16(m_buffer, m_dosTime);
    putU16(m_buffer, m_dosDate);
    putU32(m_buffer, e.crc);
    putU32(m_buffer, e.compressedSize);
    putU32(m_buffer, e.size);
    putU16(m_buffer, quint16(e.name.size()));
    putU16(m_buffer, 0);
    m_buffer.append(e.name);
    m_buffer.append(payload);
    m_entries << e;
}

QByteArray ZipWriter::finish()
{
    if (m_finished)
        return m_buffer;
    const quint32 cdOffset = quint32(m_buffer.size());
    for (const Entry &e : std::as_const(m_entries)) {
        putU32(m_buffer, 0x02014b50u);
        putU16(m_buffer, 20); // version made by (MS-DOS)
        putU16(m_buffer, 20); // version needed
        putU16(m_buffer, 0x0800);
        putU16(m_buffer, e.method);
        putU16(m_buffer, m_dosTime);
        putU16(m_buffer, m_dosDate);
        putU32(m_buffer, e.crc);
        putU32(m_buffer, e.compressedSize);
        putU32(m_buffer, e.size);
        putU16(m_buffer, quint16(e.name.size()));
        putU16(m_buffer, 0); // extra
        putU16(m_buffer, 0); // comment
        putU16(m_buffer, 0); // disk
        putU16(m_buffer, 0); // internal attributes
        putU32(m_buffer, 0); // external attributes
        putU32(m_buffer, e.offset);
        m_buffer.append(e.name);
    }
    const quint32 cdSize = quint32(m_buffer.size()) - cdOffset;
    putU32(m_buffer, 0x06054b50u);
    putU16(m_buffer, 0);
    putU16(m_buffer, 0);
    putU16(m_buffer, quint16(m_entries.size()));
    putU16(m_buffer, quint16(m_entries.size()));
    putU32(m_buffer, cdSize);
    putU32(m_buffer, cdOffset);
    putU16(m_buffer, 0);
    m_finished = true;
    return m_buffer;
}

bool ZipWriter::save(const QString &path, QString *errorMessage)
{
    const QByteArray data = finish();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (errorMessage)
            *errorMessage = tr("Dosya yazılamadı: %1").arg(file.errorString());
        return false;
    }
    return true;
}
