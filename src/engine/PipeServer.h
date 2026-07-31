#ifndef PIPESERVER_H
#define PIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>

class PipeServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY clientConnected)
public:
    explicit PipeServer(QObject *parent = nullptr);

    Q_INVOKABLE bool start(quint64 targetPid);
    Q_INVOKABLE void stop();
    bool isConnected() const { return m_client != nullptr; }

    QJsonObject sendCommand(const QJsonObject &cmd, int timeoutMs = 5000);

signals:
    void clientConnected();
    void clientDisconnected();
    void injectReady();
    void responseReceived(const QJsonObject &response);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QJsonObject makeErrorResponse(const QString &message) const;

    QLocalServer *m_server   = nullptr;
    QLocalSocket *m_client   = nullptr;
    QString m_pipeName;
    QByteArray m_readBuffer;
};

#endif // PIPESERVER_H
