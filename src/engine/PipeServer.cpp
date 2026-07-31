#include "PipeServer.h"
#include <QJsonDocument>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

PipeServer::PipeServer(QObject *parent)
    : QObject(parent)
{
}

bool PipeServer::start(quint64 targetPid)
{
    m_pipeName = QStringLiteral("QtUIAuto_%1").arg(targetPid);
    m_server = new QLocalServer(this);

    QLocalServer::removeServer(m_pipeName);

    if (!m_server->listen(m_pipeName))
    {
        qWarning() << "[PipeServer] 启动失败:" << m_server->errorString();
        return false;
    }

    connect(m_server, &QLocalServer::newConnection,
            this, &PipeServer::onNewConnection);
    qInfo() << "[PipeServer] 监听" << m_pipeName;
    return true;
}

void PipeServer::stop()
{
    if (m_client)
    {
        m_client->disconnectFromServer();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server)
    {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    qInfo() << "[PipeServer] 已停止";
}

// ═══════════════════════ 命令发送（同步等待） ═══════════════════════

QJsonObject PipeServer::sendCommand(const QJsonObject &cmd, int timeoutMs)
{
    if (!m_client)
    {
        return makeErrorResponse(QStringLiteral("Inject DLL 未连接"));
    }

    // 命令必须按 NDJSON 组帧（单行 JSON + '\n'）：注入侧按 '\n' 拆包。
    // 不带换行时仅依赖对方“整包即完整 JSON”的兼容回退，一旦两条命令粘包
    // 就会解析失败并永久滞留在对方缓冲区，导致后续所有命令失效。
    QByteArray data = QJsonDocument(cmd).toJson(QJsonDocument::Compact);
    data.append('\n');

    const qint64 written = m_client->write(data);
    // flush() 内部是 waitForWrite(0)，返回 false 只表示异步写未在 0ms 内排空，
    // 与成败无关（大负载必然如此）。成功判据只能是 write() 的返回字节数。
    m_client->flush();
    if (written != data.size())
    {
        qWarning() << "[PipeServer] 命令写入不完整:" << written << "/" << data.size()
                   << m_client->errorString();
        return makeErrorResponse(QStringLiteral("Pipe 写入不完整"));
    }

    // 使用本地事件循环等待响应
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QJsonObject response;

    auto onResponse = [&](const QJsonObject &resp)
    {
        response = resp;
        loop.quit();
    };

    QMetaObject::Connection conn = connect(this, &PipeServer::responseReceived, onResponse);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    disconnect(conn);

    if (response.isEmpty())
    {
        // 本条命令的响应将在超时之后到达，预先标记为待丢弃，
        // 否则下一条命令会把它当成自己的响应
        ++m_pendingDiscards;
        qWarning() << "[PipeServer] 命令超时:" << cmd.value("action").toString()
                   << "，待丢弃响应数:" << m_pendingDiscards;
        return makeErrorResponse(QStringLiteral("命令超时"));
    }
    return response;
}

// ═══════════════════════ Pipe 事件处理 ═══════════════════════

void PipeServer::onNewConnection()
{
    m_client = m_server->nextPendingConnection();
    if (!m_client) return;

    connect(m_client, &QLocalSocket::readyRead,
            this, &PipeServer::onReadyRead);
    connect(m_client, &QLocalSocket::disconnected,
            this, &PipeServer::onClientDisconnected);

    qInfo() << "[PipeServer] Inject DLL 已连接";
    emit clientConnected();
}

void PipeServer::onReadyRead()
{
    if (!m_client) return;

    m_readBuffer.append(m_client->readAll());

    // Responses are newline-delimited to handle multiple JSON objects
    // arriving in a single readyRead event.
    int pos = 0;
    while ((pos = m_readBuffer.indexOf('\n')) >= 0)
    {
        QByteArray line = m_readBuffer.left(pos).trimmed();
        m_readBuffer.remove(0, pos + 1);
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
        {
            qWarning() << "[PipeServer] JSON parse failed:" << err.errorString();
            continue;
        }

        QJsonObject msg = doc.object();

        // inject_ready 通知（连接后立即发送）
        if (msg.value("action").toString() == "inject_ready")
        {
            qInfo() << "[PipeServer] Inject DLL ready";
            emit injectReady();
            continue;
        }

        // 已超时命令的迟到响应：必须在此丢弃，不能当作当前命令的响应向上报
        if (m_pendingDiscards > 0)
        {
            --m_pendingDiscards;
            qWarning() << "[PipeServer] 丢弃已超时命令的迟到响应:"
                       << msg.value("action").toString();
            continue;
        }

        // 正常命令响应
        emit responseReceived(msg);
    }
}

void PipeServer::onClientDisconnected()
{
    qInfo() << "[PipeServer] Inject DLL 断开连接";
    m_client = nullptr;
    // 连接已断，迟到响应不可能再到达；不清零会让重连后的首条响应被误丢
    m_pendingDiscards = 0;
    m_readBuffer.clear();
    emit clientDisconnected();
}

// ═══════════════════════ 辅助 ═══════════════════════

QJsonObject PipeServer::makeErrorResponse(const QString &message) const
{
    QJsonObject resp;
    resp["status"]  = QStringLiteral("error");
    resp["message"] = message;
    return resp;
}
