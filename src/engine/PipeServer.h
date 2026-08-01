#ifndef PIPESERVER_H
#define PIPESERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>

/// ============================================================
/// PipeServer — 命名管道服务端（通信层）
///
/// 职责边界：
///   - 以目标进程 PID 命名管道并监听，等待注入侧回连
///   - 同步收发 JSON 命令与响应：sendCommand() 内起事件循环等待，
///     调用方拿到的就是配对好的响应
///   - 收发一律按 NDJSON 组帧（单行 JSON + '\n'）；不带换行时两条
///     命令粘包就会让对方解析失败并永久滞留在缓冲区
///   - 命令超时后其响应仍会迟到，必须计数丢弃，否则会被下一条命令
///     的事件循环抢走，造成请求与响应错位
///   - 不解释命令内容，只负责搬运
/// ============================================================
class PipeServer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY clientConnected)

public:
    /// 构造管道服务端
    /// @param parent Qt 父对象
    explicit PipeServer(QObject *parent = nullptr);

    // ── 生命周期 ──

    /// 按目标进程 PID 生成管道名并开始监听
    /// @param targetPid 目标进程 PID，用于构造唯一管道名
    /// @return 是否监听成功
    Q_INVOKABLE bool start(quint64 targetPid);

    /// 停止监听并断开已有连接
    Q_INVOKABLE void stop();

    /// @return 注入侧是否已回连
    bool isConnected() const { return m_client != nullptr; }

    // ── 命令收发 ──

    /// 同步下发一条命令并等待其响应
    /// @param cmd 命令内容，须含 action 字段
    /// @param timeoutMs 等待响应的超时毫秒数
    /// @return 响应 JSON；超时或未连接时返回 status 为 error 的错误响应
    QJsonObject sendCommand(const QJsonObject &cmd, int timeoutMs = 5000);

signals:
    // ═══════════ 信号 ═══════════

    /// 注入侧已连上管道
    void clientConnected();

    /// 注入侧已断开
    void clientDisconnected();

    /// 注入侧发来就绪握手，表示 agent 初始化完成、可接受命令
    void injectReady();

    /// 收到一条非握手响应
    /// @param response 响应 JSON
    void responseReceived(const QJsonObject &response);

private slots:
    // ── 套接字事件 ──

    /// 接受新连接
    void onNewConnection();

    /// 读取并解析管道数据
    void onReadyRead();

    /// 处理连接断开
    void onClientDisconnected();

private:
    // ── 私有实现 ──

    /// 构造统一格式的错误响应
    /// @param message 错误描述
    /// @return status 为 error 的响应 JSON
    QJsonObject makeErrorResponse(const QString &message) const;

    QLocalServer *m_server   = nullptr;
    QLocalSocket *m_client   = nullptr;
    QString m_pipeName;
    QByteArray m_readBuffer;

    // 已超时命令的数量。超时返回后其响应仍会迟到，若不丢弃就会被下一条
    // 命令的事件循环抢到，造成请求与响应错位。
    int m_pendingDiscards = 0;
};

#endif // PIPESERVER_H
