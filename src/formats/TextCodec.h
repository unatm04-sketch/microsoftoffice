#pragma once

#include <QByteArray>
#include <QString>

namespace TextCodec {

// Decodes text files: honours BOMs, validates UTF-8 and falls back to the
// Turkish Windows code page (1254) for legacy files.
QString decode(const QByteArray &data);
// Decodes data in a named charset (MIME, HTML). Unknown charsets fall back to decode().
QString decode(const QByteArray &data, const QByteArray &charset);

} // namespace TextCodec
