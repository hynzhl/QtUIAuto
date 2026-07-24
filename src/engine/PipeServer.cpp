#include "PipeServer.h"
#include "AccessibilityController.h"
#include <QJsonDocument>
#include <QDebug>

PipeServer::PipeServer(quint16 port, QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(port)
    m_controller = new AccessibilityController(this);
}

bool PipeServer::start(quint64 targetPid)
{
    m_pipeName = QString("QtUIAuto_%1").arg(targetPid);
    m_server = new QLocalServer(this);

    // Remove existing pipe if any
    QLocalServer::removeServer(m_pipeName);

    if (!m_server->listen(m_pipeName)) {
        qWarning() << "PipeServer: Failed to start:" << m_server->errorString();
        return false;
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &PipeServer::onNewConnection);
    qInfo() << "PipeServer: Listening on" << m_pipeName;
    return true;
}

void PipeServer::stop()
{
    if (m_client) m_client->disconnectFromServer();
    if (m_server) m_server->close();
}

void PipeServer::onNewConnection()
{
    m_client = m_server->nextPendingConnection();
    connect(m_client, &QLocalSocket::readyRead,
            this, &PipeServer::onReadyRead);
    connect(m_client, &QLocalSocket::disconnected,
            this, &PipeServer::onClientDisconnected);
    emit clientConnected();
}

void PipeServer::onReadyRead()
{
    QByteArray data = m_client->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "PipeServer: Invalid JSON received";
        return;
    }

    QJsonObject response = m_controller->executeCommand(doc.object());
    m_client->write(QJsonDocument(response).toJson(QJsonDocument::Compact));
    m_client->flush();
}

void PipeServer::onClientDisconnected()
{
    m_client = nullptr;
    emit clientDisconnected();
}
