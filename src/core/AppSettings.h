#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>

// Application wide preferences and the recent file list.
class AppSettings : public QObject
{
    Q_OBJECT
public:
    static AppSettings &instance();

    QSettings &settings() { return m_settings; }

    QStringList recentFiles() const;
    void addRecentFile(const QString &path);
    void removeRecentFile(const QString &path);
    void clearRecentFiles();
    bool isPinned(const QString &path) const;
    void setPinned(const QString &path, bool pinned);

    QString userName() const;
    void setUserName(const QString &name);

    QString lastDirectory() const;
    void setLastDirectory(const QString &directory);

    int autoRecoverMinutes() const; // 0 disables auto recovery
    void setAutoRecoverMinutes(int minutes);

    bool restoreSession() const;
    void setRestoreSession(bool restore);

    bool showStartPage() const;
    void setShowStartPage(bool show);

signals:
    void recentFilesChanged();
    void settingsChanged();

private:
    AppSettings();

    QSettings m_settings;
};
