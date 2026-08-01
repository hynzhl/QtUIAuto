#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <memory>

/// ============================================================
/// ProcessManager — 目标进程启动与 DLL 注入管理器（业务逻辑层）
///
/// 职责边界：
///   - 拉起被测程序并跟踪其存活状态
///   - 以 SetWindowsHookEx 方式把 agent DLL 送入目标进程：钩子必须装到
///     目标的 GUI 线程上，所以调用方需等窗口就绪后再注入
///   - 不做跨进程内存写入（VirtualAllocEx / CreateRemoteThread），避免
///     被 EDR / HIPS 拦截
///   - 不与注入侧通信：通信由 PipeServer 负责
/// ============================================================
class ProcessManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(quint64 targetPid READ targetPid NOTIFY targetStarted)
    Q_PROPERTY(QString injectDllPath READ injectDllPath WRITE setInjectDllPath)
    Q_PROPERTY(bool running READ isRunning NOTIFY targetStopped)

public:
    /// 构造进程管理器
    /// @param parent Qt 父对象
    explicit ProcessManager(QObject *parent = nullptr);

    /// 析构时释放钩子并卸载本地 DLL 模块
    ~ProcessManager() override;

    // ── 进程与注入 ──

    /// 启动被测程序
    /// @param appPath 可执行文件路径
    /// @param args 传给被测程序的命令行参数
    /// @return 进程是否成功启动
    Q_INVOKABLE bool launchTarget(const QString &appPath, const QStringList &args = {});

    /// 向已启动的目标进程注入 agent DLL。须在目标窗口就绪后调用。
    /// @return 钩子是否成功装载；实际注入结果另经 injectionResult 上报
    Q_INVOKABLE bool injectDll();

    /// 停止目标进程并释放钩子
    Q_INVOKABLE void stopTarget();

    // ── 状态读取 ──

    /// @return 目标进程 PID；未启动时为 0
    quint64 targetPid() const { return m_targetPid; }

    /// @return 目标进程是否仍在运行
    bool isRunning() const;

    /// @return 当前使用的 agent DLL 路径
    QString injectDllPath() const { return m_injectDllPath; }

    /// 指定 agent DLL 路径，留空则按约定位置自动解析
    /// @param path DLL 完整路径
    void setInjectDllPath(const QString &path) { m_injectDllPath = path; }

signals:
    // ═══════════ 信号 ═══════════

    /// 目标进程已启动
    /// @param pid 目标进程 PID
    void targetStarted(quint64 pid);

    /// 目标进程已退出
    void targetStopped();

    /// 注入结果
    /// @param success 是否注入成功
    /// @param message 失败原因，成功时为空
    void injectionResult(bool success, const QString &message);

private:
    // ── 私有实现 ──

    /// 按约定位置推导 agent DLL 路径
    /// @return DLL 完整路径；找不到时为空串
    QString resolveDllPath() const;

    /// 卸载钩子并释放本地 DLL 模块引用
    void releaseHook();

    // 目标进程句柄。QProcess 为 QObject 派生，按规范 §7 用 QPointer 持有。
    QPointer<QProcess> m_process;
    quint64 m_targetPid = 0;
    QString m_targetPath;
    QString m_injectDllPath;

    // WH_GETMESSAGE 钩子与本地 DLL 模块句柄封装为 RAII，析构时自动释放。
    // 类型前向声明以避免把 windows.h 的宏污染到每个包含者；
    // 因 MSVC 对 incomplete type 的 unique_ptr 默认删除器较严格，故使用自定义删除器。
    struct HookHandle;
    struct HookHandleDeleter
    {
        void operator()(HookHandle *ptr) const;
    };
    std::unique_ptr<HookHandle, HookHandleDeleter> m_hookHandle;
};

#endif // PROCESSMANAGER_H
