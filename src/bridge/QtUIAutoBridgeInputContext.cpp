#include "QtUIAutoBridgeInputContext.h"
#include "ControlScanner.h"
#include "InputSimulator.h"
#include "CommandHandler.h"

#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QTimer>

#include <windows.h>

static void bridgeLog(const char *msg)
{
    OutputDebugStringA("[QtUIAutoBridge] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
    qDebug() << "[QtUIAutoBridge]" << msg;
}

QtUIAutoBridgeInputContext::QtUIAutoBridgeInputContext()
{
    // Defer actual initialization until the event loop is ready.
    QTimer::singleShot(0, this, &QtUIAutoBridgeInputContext::initAgent);
}

QtUIAutoBridgeInputContext::~QtUIAutoBridgeInputContext()
{
    if (m_pipe)
    {
        m_pipe->disconnectFromServer();
        m_pipe->deleteLater();
    }
}

void QtUIAutoBridgeInputContext::initAgent()
{
    bridgeLog("initAgent start");

    m_scanner = new ControlScanner(this);
    m_simulator = new InputSimulator(this);
    m_handler = new CommandHandler(m_scanner, m_simulator, this);

    const DWORD pid = GetCurrentProcessId();
    const QString pipeName = QStringLiteral("QtUIAuto_%1").arg(pid);
    bridgeLog(qPrintable(QStringLiteral("Connecting to ") + pipeName));

    m_pipe = new QLocalSocket(this);
    connect(m_pipe, &QLocalSocket::readyRead, this, &QtUIAutoBridgeInputContext::onReadyRead);
    connect(m_pipe, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, [](QLocalSocket::LocalSocketError err) {
        qWarning() << "[QtUIAutoBridge] Pipe error:" << err;
    });
    connect(m_pipe, &QLocalSocket::disconnected, this, &QtUIAutoBridgeInputContext::onDisconnected);

    m_pipe->connectToServer(pipeName, QIODevice::ReadWrite);
    if (!m_pipe->waitForConnected(5000))
    {
        qWarning() << "[QtUIAutoBridge] Pipe connect failed:" << pipeName;
        return;
    }

    bridgeLog("Pipe connected OK");
    sendInjectReady();
}

void QtUIAutoBridgeInputContext::sendInjectReady()
{
    if (!m_pipe)
        return;

    QJsonObject hello;
    hello["action"] = QStringLiteral("inject_ready");
    m_pipe->write(QJsonDocument(hello).toJson(QJsonDocument::Compact));
    m_pipe->flush();
    bridgeLog("inject_ready sent");
}

void QtUIAutoBridgeInputContext::onReadyRead()
{
    if (!m_pipe || !m_handler)
        return;

    const QByteArray data = m_pipe->readAll();
    if (data.isEmpty())
        return;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "[QtUIAutoBridge] JSON parse error:" << err.errorString();
        return;
    }

    const QJsonObject response = m_handler->handleCommand(doc.object());
    m_pipe->write(QJsonDocument(response).toJson(QJsonDocument::Compact));
    m_pipe->flush();
}

void QtUIAutoBridgeInputContext::onDisconnected()
{
    bridgeLog("Pipe disconnected");
}
