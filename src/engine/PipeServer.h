#ifndef PIPESERVER_H
#define PIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>

class AccessibilityController;

class PipeServer : public QObject
{
    Q_OBJECT
public:
    explicit PipeServer(quint16 port = 0, QObject *parent = nullptr);
    bool start(quint64 targetPid);
    void stop();

signals:
    void clientConnected();
    void clientDisconnected();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QLocalServer *m_server = nullptr;
    QLocalSocket *m_client = nullptr;
    AccessibilityController *m_controller = nullptr;
    QString m_pipeName;
};

#endif // PIPESERVER_H
