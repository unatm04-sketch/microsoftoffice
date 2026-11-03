#include "core/AppSettings.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
constexpr int kMaxRecentFiles = 40;
const char kRecentKey[] = "files/recent";
const char kPinnedKey[] = "files/pinned";
} // namespace

AppSettings &AppSettings::instance()
{
    static AppSettings settings;
    return settings;
}

AppSettings::AppSettings() = default;

QStringList AppSettings::recentFiles() const
{
    QStringList files = m_settings.value(kRecentKey).toStringList();
    const QStringList pinned = m_settings.value(kPinnedKey).toStringList();
    // Pinned files first, keeping their relative order.
    QStringList result;
    for (const QString &f : files) {
        if (pinned.contains(f))
            result << f;
    }
    for (const QString &f : files) {
        if (!pinned.contains(f))
            result << f;
    }
    return result;
}

void AppSettings::addRecentFile(const QString &path)
{
    if (path.isEmpty())
        return;
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QStringList files = m_settings.value(kRecentKey).toStringList();
    files.removeAll(clean);
    files.prepend(clean);
    while (files.size() > kMaxRecentFiles)
        files.removeLast();
    m_settings.setValue(kRecentKey, files);
    emit recentFilesChanged();
}

void AppSettings::removeRecentFile(const QString &path)
{
    QStringList files = m_settings.value(kRecentKey).toStringList();
    if (files.removeAll(path) == 0)
        return;
    m_settings.setValue(kRecentKey, files);
    setPinned(path, false);
    emit recentFilesChanged();
}

void AppSettings::clearRecentFiles()
{
    const QStringList pinned = m_settings.value(kPinnedKey).toStringList();
    m_settings.setValue(kRecentKey, pinned);
    emit recentFilesChanged();
}

bool AppSettings::isPinned(const QString &path) const
{
    return m_settings.value(kPinnedKey).toStringList().contains(path);
}

void AppSettings::setPinned(const QString &path, bool pinned)
{
    QStringList list = m_settings.value(kPinnedKey).toStringList();
    list.removeAll(path);
    if (pinned)
        list.prepend(path);
    m_settings.setValue(kPinnedKey, list);
    emit recentFilesChanged();
}

QString AppSettings::userName() const
{
    QString name = m_settings.value(QStringLiteral("general/userName")).toString();
    if (name.isEmpty())
        name = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER"));
    return name;
}

void AppSettings::setUserName(const QString &name)
{
    m_settings.setValue(QStringLiteral("general/userName"), name);
    emit settingsChanged();
}

QString AppSettings::lastDirectory() const
{
    const QString dir = m_settings.value(QStringLiteral("files/lastDirectory")).toString();
    if (!dir.isEmpty() && QFileInfo(dir).isDir())
        return dir;
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void AppSettings::setLastDirectory(const QString &directory)
{
    m_settings.setValue(QStringLiteral("files/lastDirectory"), directory);
}

int AppSettings::autoRecoverMinutes() const
{
    return m_settings.value(QStringLiteral("general/autoRecoverMinutes"), 5).toInt();
}

void AppSettings::setAutoRecoverMinutes(int minutes)
{
    m_settings.setValue(QStringLiteral("general/autoRecoverMinutes"), qMax(0, minutes));
    emit settingsChanged();
}

bool AppSettings::restoreSession() const
{
    return m_settings.value(QStringLiteral("general/restoreSession"), false).toBool();
}

void AppSettings::setRestoreSession(bool restore)
{
    m_settings.setValue(QStringLiteral("general/restoreSession"), restore);
    emit settingsChanged();
}

bool AppSettings::showStartPage() const
{
    return m_settings.value(QStringLiteral("general/showStartPage"), true).toBool();
}

void AppSettings::setShowStartPage(bool show)
{
    m_settings.setValue(QStringLiteral("general/showStartPage"), show);
    emit settingsChanged();
}
