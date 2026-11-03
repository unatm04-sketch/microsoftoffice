#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

// Ensures a single running instance; later launches forward their files to it.
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);

    // Returns true when another instance accepted the files (this process should exit).
    bool forwardToRunningInstance(const QStringList &files);
    void listen();

signals:
    void filesReceived(const QStringList &files);

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};
