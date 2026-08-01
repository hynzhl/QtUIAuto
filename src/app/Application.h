#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QPointer>

class ProcessManager;
class PipeServer;
class ControlTree;
class ScriptEngine;
class AppContext;

/// ============================================================
/// Application — 宿主程序入口与对象图根（应用层）
///
/// 职责边界：
///   - 构造并持有四个引擎，以及封装它们的 AppContext 门面
///   - 串起自动注入流程：进程启动 → 开管道 → 等窗口就绪 → 注入 DLL
///   - 加载 QML 引擎，并且只向 QML 暴露一个上下文属性 appContext
///   - 不包含业务逻辑：一切动作均转发给引擎或门面
/// ============================================================
class Application : public QApplication
{
    Q_OBJECT

public:
    /// 构造应用：建立引擎、接好自动注入链路并加载 QML 根组件
    /// @param argc 命令行参数个数，按 QApplication 要求传引用
    /// @param argv 命令行参数数组
    Application(int &argc, char **argv);

    ~Application() override;

private:
    // ── 私有实现 ──

    /// 向 QML 上下文注册门面对象
    void setupQmlContext();

    // 核心组件均为 QObject 派生且由 Qt 对象树管理，按规范 §7 用 QPointer 持有。
    QPointer<QQmlApplicationEngine> m_engine;
    QPointer<ProcessManager>        m_process;
    QPointer<PipeServer>            m_pipe;
    QPointer<ControlTree>           m_tree;
    QPointer<ScriptEngine>          m_script;

    // 暴给 QML 的唯一对象。上面四个引擎仅由 C++ 侧持有。
    QPointer<AppContext>            m_context;
};

#endif // APPLICATION_H
