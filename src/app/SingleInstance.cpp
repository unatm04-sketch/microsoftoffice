#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
    const QByteArray user = qgetenv("USERNAME").isEmpty() ? qgetenv("USER") : qgetenv("USERNAME");
    m_name = QStringLiteral("OmniOffice-")
             + QString::fromLatin1(QCryptographicHash::hash(user, QCryptographicHash::Sha1).toHex().left(12));
}

bool SingleInstance::forwardToRunningInstance(const QStringList &files)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(300))
        return false;
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    QStringList absolute;
    for (const QString &f : files)
        absolute << QDir::current().absoluteFilePath(f);
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out << absolute;
    socket.write(payload);
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    return true;
}

void SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    QLocalServer::removeServer(m_name);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_name))
        return;
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                const QByteArray data = socket->readAll();
                QDataStream in(data);
                QStringList files;
                in >> files;
                emit filesReceived(files);
            });
        }
    });
}
