#ifndef APPCONTEXT_H
#define APPCONTEXT_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

class ProcessManager;
class PipeServer;
class ControlTree;
class ScriptEngine;

/// ============================================================
/// AppContext — 业务门面（Facade），QML 层唯一交互入口（规范 §8 / §10）
///
/// QML 不得持有 ProcessManager / PipeServer / ControlTree / ScriptEngine
/// 中的任何一个：状态一律经本类的 Q_PROPERTY 读取，动作一律经
/// Q_INVOKABLE 下发，底层引擎的信号在此转发为门面信号。
///
/// 脚本文件路径同样由本类决定——QML 不接触文件系统，也就不会再出现
/// 把 "C:/xxx.json" 写死在界面里的情形。
/// ============================================================
class AppContext : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool recording READ isRecording NOTIFY scriptStateChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY scriptStateChanged)
    Q_PROPERTY(QVariantList script READ script NOTIFY scriptStateChanged)

public:
    /// 构造门面并接管底层引擎的信号转发
    /// @param process 目标进程管理器（启动 / 注入 / 停止）
    /// @param pipe 命名管道服务端（与注入 DLL 的通信通道）
    /// @param tree 控件树查询器
    /// @param script 脚本录制与回放引擎
    /// @param parent Qt 父对象
    AppContext(ProcessManager *process,
               PipeServer *pipe,
               ControlTree *tree,
               ScriptEngine *script,
               QObject *parent = nullptr);

    // ── 状态读取 ──

    /// @return 注入 DLL 是否已连上管道
    bool isConnected() const;

    /// @return 是否处于录制状态
    bool isRecording() const;

    /// @return 是否处于回放状态
    bool isPlaying() const;

    /// @return 当前脚本的步骤列表，供 QML 列表视图消费
    QVariantList script() const;

    // ── 目标进程 ──

    /// 启动被测程序
    /// @param appPath 可执行文件路径
    /// @return 进程是否成功启动
    Q_INVOKABLE bool launchTarget(const QString &appPath);

    /// 向已启动的目标进程注入 DLL
    /// @return 注入请求是否成功发起
    Q_INVOKABLE bool injectDll();

    /// 停止目标进程
    Q_INVOKABLE void stopTarget();

    // ── 录制与回放 ──

    /// 切换录制状态：录制中则停止，否则开始。
    /// 状态分支放在此处而非 QML，是为了让界面只负责展示（规范 §8）。
    Q_INVOKABLE void toggleRecording();

    /// 切换回放状态：回放中则停止，否则开始
    Q_INVOKABLE void togglePlayback();

    // ── 脚本读写 ──

    /// 把当前脚本保存到应用数据目录
    /// @return 是否保存成功；失败时 scriptSaved 信号同样会带 false 发出
    Q_INVOKABLE bool saveScript();

    /// 从应用数据目录读取脚本
    /// @return 是否加载成功
    Q_INVOKABLE bool loadScript();

    /// @return 脚本文件的完整路径，供界面展示给用户
    Q_INVOKABLE QString scriptFilePath() const;

    // ── 控件树 ──

    /// 拉取目标进程的根窗口列表
    /// @return 每个元素为含 objectName / typeName 等字段的 QVariantMap
    Q_INVOKABLE QVariantList rootWindowList();

signals:
    // ═══════════ 信号 ═══════════

    /// 管道连接状态发生变化（连上或断开）
    void connectionChanged();

    /// DLL 注入结果
    /// @param success 是否注入成功
    /// @param message 失败原因，成功时为空
    void injectionResult(bool success, const QString &message);

    /// 录制 / 回放状态或脚本内容发生变化
    void scriptStateChanged();

    /// 回放进度推进
    /// @param step 当前步序号（从 1 起）
    /// @param total 总步数
    void playbackStep(int step, int total);

    /// 回放结束
    /// @param success 是否全部步骤成功
    void playbackFinished(bool success);

    /// 脚本保存完毕
    /// @param success 是否成功
    /// @param path 目标文件路径
    void scriptSaved(bool success, const QString &path);

    /// 脚本加载完毕
    /// @param success 是否成功
    /// @param path 来源文件路径
    void scriptLoaded(bool success, const QString &path);

private:
    // ── 私有实现 ──

    /// 确保脚本目录存在
    /// @return 目录是否可用
    bool ensureScriptDir() const;

    // 底层引擎均为 QObject 派生且生命周期由 Qt 对象树管理，按规范 §7
    // 用 QPointer 持有：任一引擎先于本类析构时自动置空，调用点判空即安全。
    QPointer<ProcessManager> m_process;
    QPointer<PipeServer>     m_pipe;
    QPointer<ControlTree>    m_tree;
    QPointer<ScriptEngine>   m_script;
};

#endif // APPCONTEXT_H
