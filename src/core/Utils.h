#pragma once

#include <QDateTime>
#include <QString>

class QWidget;

namespace Utils {

QString humanSize(qint64 bytes);
// Case and diacritic insensitive key, aware of Turkish letters (İ/ı, Ş, Ğ...).
QString searchKey(const QString &text);
bool matchesSearch(const QString &haystack, const QString &needle);
QString relativeTime(const QDateTime &time);
// Extension (without dot) of the first pattern in a file dialog filter.
QString extensionFromFilter(const QString &filter);
// Appends the extension implied by `selectedFilter` when `path` has none.
QString ensureExtension(const QString &path, const QString &selectedFilter, const QString &fallbackExtension);
// Shows a file in the platform file manager.
void revealInFileManager(const QString &path);

} // namespace Utils
