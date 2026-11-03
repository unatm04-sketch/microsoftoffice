#include "formats/TextCodec.h"

#include <QStringDecoder>

namespace {

// Code points of Windows-1252 bytes 0x80-0x9F (0 = undefined).
const char16_t kCp1252High[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0, 0x0178};

char16_t turkishLatin(uchar b)
{
    switch (b) {
    case 0xD0: return 0x011E; // Ğ
    case 0xDD: return 0x0130; // İ
    case 0xDE: return 0x015E; // Ş
    case 0xF0: return 0x011F; // ğ
    case 0xFD: return 0x0131; // ı
    case 0xFE: return 0x015F; // ş
    default: return b;
    }
}

QString decodeSingleByte(const QByteArray &data, bool windows)
{
    QString out;
    out.reserve(data.size());
    for (const char c : data) {
        const uchar b = uchar(c);
        if (b < 0x80) {
            out += QChar(b);
        } else if (b < 0xA0) {
            const char16_t cp = windows ? kCp1252High[b - 0x80] : char16_t(b);
            out += QChar(cp ? cp : 0xFFFD);
        } else {
            out += QChar(turkishLatin(b));
        }
    }
    return out;
}

} // namespace

namespace TextCodec {

QString decode(const QByteArray &data)
{
    if (data.startsWith("\xEF\xBB\xBF"))
        return QString::fromUtf8(data.mid(3));
    if (data.startsWith("\xFF\xFE")) {
        QStringDecoder d(QStringDecoder::Utf16LE);
        return d.decode(data.mid(2));
    }
    if (data.startsWith("\xFE\xFF")) {
        QStringDecoder d(QStringDecoder::Utf16BE);
        return d.decode(data.mid(2));
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = utf8.decode(data);
    if (!utf8.hasError())
        return text;
    return decodeSingleByte(data, true);
}

QString decode(const QByteArray &data, const QByteArray &charset)
{
    const QByteArray cs = charset.trimmed().toLower();
    if (cs.isEmpty())
        return decode(data);
    if (cs == "windows-1254" || cs == "cp1254" || cs == "x-cp1254")
        return decodeSingleByte(data, true);
    if (cs == "iso-8859-9" || cs == "latin5" || cs == "iso8859-9")
        return decodeSingleByte(data, false);
    if (cs == "us-ascii" || cs == "ascii")
        return QString::fromLatin1(data);
    QStringDecoder decoder(cs.constData());
    if (decoder.isValid()) {
        const QString text = decoder.decode(data);
        if (!decoder.hasError())
            return text;
    }
    return decode(data);
}

} // namespace TextCodec
