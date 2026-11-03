#include "core/Utils.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

namespace Utils {

QString humanSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

QString searchKey(const QString &text)
{
    QString key;
    key.reserve(text.size());
    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case 0x0130: // İ
        case 0x0131: // ı
        case 'I':
            key += QLatin1Char('i');
            continue;
        case 0x015E: case 0x015F: key += QLatin1Char('s'); continue; // Ş ş
        case 0x011E: case 0x011F: key += QLatin1Char('g'); continue; // Ğ ğ
        case 0x00DC: case 0x00FC: key += QLatin1Char('u'); continue; // Ü ü
        case 0x00D6: case 0x00F6: key += QLatin1Char('o'); continue; // Ö ö
        case 0x00C7: case 0x00E7: key += QLatin1Char('c'); continue; // Ç ç
        default:
            break;
        }
        const QString decomposed = QString(ch).normalized(QString::NormalizationForm_D);
        key += decomposed.isEmpty() ? ch.toLower() : decomposed.at(0).toLower();
    }
    return key;
}

bool matchesSearch(const QString &haystack, const QString &needle)
{
    if (needle.isEmpty())
        return true;
    return searchKey(haystack).contains(searchKey(needle));
}

QString relativeTime(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 secs = time.secsTo(now);
    if (secs < 60)
        return QCoreApplication::translate("Utils", "Az önce");
    if (secs < 3600)
        return QCoreApplication::translate("Utils", "%1 dk önce").arg(secs / 60);
    if (time.date() == now.date())
        return QCoreApplication::translate("Utils", "Bugün %1").arg(QLocale().toString(time.time(), QLocale::ShortFormat));
    if (time.date() == now.date().addDays(-1))
        return QCoreApplication::translate("Utils", "Dün %1").arg(QLocale().toString(time.time(), QLocale::ShortFormat));
    return QLocale().toString(time.date(), QLocale::ShortFormat);
}

QString extensionFromFilter(const QString &filter)
{
    static const QRegularExpression re(QStringLiteral("\\*\\.([A-Za-z0-9]+)"));
    const QRegularExpressionMatch m = re.match(filter);
    return m.hasMatch() ? m.captured(1).toLower() : QString();
}

QString ensureExtension(const QString &path, const QString &selectedFilter, const QString &fallbackExtension)
{
    if (path.isEmpty() || !QFileInfo(path).suffix().isEmpty())
        return path;
    QString ext = extensionFromFilter(selectedFilter);
    if (ext.isEmpty())
        ext = fallbackExtension;
    return ext.isEmpty() ? path : path + QLatin1Char('.') + ext;
}

void revealInFileManager(const QString &path)
{
    const QFileInfo info(path);
#ifdef Q_OS_WIN
    if (info.exists()) {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                {QStringLiteral("/select,"), QDir::toNativeSeparators(info.absoluteFilePath())});
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
}

} // namespace Utils
