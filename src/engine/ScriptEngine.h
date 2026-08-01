#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QVariantList>

class PipeServer;

/// ============================================================
/// ScriptEngine — 脚本录制与回放引擎（业务逻辑层）
///
/// 职责边界：
///   - 维护录制 / 回放状态机（Idle / Recording / Playing / Paused）
///   - 持有步骤序列的内存表示，并负责与 JSON 文件互转
///   - 回放由定时器逐步驱动而非阻塞等待，事件循环因此保持可响应，
///     stopPlayback() 才能真正中止链条
///   - 不决定脚本文件放在哪里：路径一律由调用方（AppContext）给出
/// ============================================================
class ScriptEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList script READ scriptVariant NOTIFY stateChanged)
    // 以布尔形式另行暴露状态：调用方（含门面与界面）因此无需
    // 按类型名读枚举，也就不必让 QML 认识 ScriptEngine 这个 C++ 类型。
    Q_PROPERTY(bool recording READ isRecording NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)

public:
    /// 引擎状态。Paused 仅由 pausePlayback() 产生，保留回放进度。
    enum State { Idle, Recording, Playing, Paused };
    Q_ENUM(State)

    /// 构造脚本引擎
    /// @param pipeServer 命名管道服务端，回放时经它向目标进程下发命令；其生命周期由调用方保证
    /// @param parent Qt 父对象
    explicit ScriptEngine(PipeServer *pipeServer, QObject *parent = nullptr);

    // ── 状态读取 ──

    /// @return 当前状态枚举
    State state() const { return m_state; }

    /// @return 是否处于录制状态
    bool isRecording() const { return m_state == Recording; }

    /// @return 是否处于回放状态
    bool isPlaying() const { return m_state == Playing; }

    // ── 录制 ──

    /// 开始录制。会清空已有步骤序列。
    Q_INVOKABLE void startRecording();

    /// 停止录制，状态回到 Idle。已录制的步骤保留在内存中。
    Q_INVOKABLE void stopRecording();

    /// 追加一条录制步骤；非录制状态下调用会被静默忽略
    /// @param event 步骤内容，含 action 字段及其参数
    Q_INVOKABLE void recordEvent(const QJsonObject &event);

    // ── 回放 ──

    /// 开始回放当前脚本。步骤为空或管道未连接时立即以失败收场。
    Q_INVOKABLE void startPlayback();

    /// 中止回放，状态回到 Idle
    Q_INVOKABLE void stopPlayback();

    /// 暂停回放并保留当前进度
    Q_INVOKABLE void pausePlayback();

    // ── 脚本读写 ──

    /// 从 JSON 文件读取脚本，成功时替换当前步骤序列
    /// @param filePath 脚本文件完整路径
    /// @return 是否读取并解析成功
    Q_INVOKABLE bool loadScript(const QString &filePath);

    /// 直接以内存中的步骤列表替换当前脚本
    /// @param steps 步骤列表，元素须为可转成 QJsonObject 的 map
    /// @return 是否替换成功
    Q_INVOKABLE bool loadScriptFromJson(const QVariantList &steps);

    /// 把当前脚本写入 JSON 文件。不负责创建目录，调用方须先保证其存在。
    /// @param filePath 目标文件完整路径
    /// @return 是否写入成功
    Q_INVOKABLE bool saveScript(const QString &filePath) const;

    /// @return 当前脚本的 JSON 数组形式
    QJsonArray script() const { return m_steps; }

    /// @return 当前脚本的 QVariantList 形式，供列表视图消费
    QVariantList scriptVariant() const;

signals:
    // ═══════════ 信号 ═══════════

    /// 状态发生变化
    /// @param state 变化后的新状态
    void stateChanged(State state);

    /// 回放进度推进
    /// @param step 已完成的步序号（从 1 起）
    /// @param total 总步数
    void playbackStep(int step, int total);

    /// 回放结束
    /// @param success 是否全部步骤都执行成功
    void playbackFinished(bool success);

    /// 录制结果已落盘
    /// @param path 脚本文件路径
    void recordingSaved(const QString &path);

private:
    // ── 私有实现 ──

    /// 执行当前步骤并投递后继步骤，形成逐步驱动的链条
    void runNextStep();

    /// 收束回放：状态置回 Idle 并发出 playbackFinished
    /// @param success 本次回放是否成功
    void finishPlayback(bool success);

    State m_state = Idle;
    QJsonArray m_steps;
    // 管道服务端为 QObject 派生，按规范 §7 用 QPointer 持有，调用点判空。
    QPointer<PipeServer> m_pipeServer;

    // 当前待执行的步骤下标。回放由定时器逐步驱动，不能用局部变量保存进度。
    int m_currentStep = 0;
};

#endif // SCRIPTENGINE_H
