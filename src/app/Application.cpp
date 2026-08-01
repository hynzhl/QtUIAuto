#include "Application.h"
#include "app/AppContext.h"
#include "app/ProcessManager.h"
#include "engine/PipeServer.h"
#include "engine/ControlTree.h"
#include "engine/ScriptEngine.h"
#include <QQmlContext>
#include <QTimer>
#include <QDebug>

namespace {
// 目标进程启动到注入之间的等待时长。钩子需要装到 GUI 线程上，
// 而 targetStarted 只说明进程已拉起，此时窗口与 QML 引擎可能尚未创建，
// 立即注入会拉不到正确的线程。E2E 侧同样采用 1500ms 等待。
constexpr int kInjectDelayMs = 1500;
}

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName("QU");
    setApplicationVersion("0.1.0");
    setOrganizationName("hynzhl");

    // 创建核心组件
    m_process = new ProcessManager(this);
    m_pipe    = new PipeServer(this);
    m_tree    = new ControlTree(m_pipe, this);
    m_script  = new ScriptEngine(m_pipe, this);

    // 业务门面：聚合上述引擎，对 QML 只暴露它一个
    m_context = new AppContext(m_process, m_pipe, m_tree, m_script, this);

    // 连接自动注入流程：进程启动 → 启动 PipeServer → 等窗口就绪 → 注入 DLL
    connect(m_process, &ProcessManager::targetStarted, this, [this](quint64 pid)
    {
        qInfo() << "[Application] 目标进程已启动, PID:" << pid;
        m_pipe->start(pid);
        QTimer::singleShot(kInjectDelayMs, this, [this]()
        {
            if (!m_process->isRunning())
            {
                qWarning() << "[Application] 目标进程已退出, 取消注入";
                return;
            }
            m_process->injectDll();
        });
    });

    connect(m_pipe, &PipeServer::clientConnected, this, [this]()
    {
        qInfo() << "[Application] Agent 已连接";
    });

    // 启动 QML 引擎
    m_engine = new QQmlApplicationEngine(this);
    setupQmlContext();

    m_engine->load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (m_engine->rootObjects().isEmpty())
    {
        qCritical() << "[Application] QML 加载失败";
    }
}

Application::~Application()
{
    if (m_engine) delete m_engine;
}

void Application::setupQmlContext()
{
    // 只暴露门面（规范 §8）。若把四个引擎平铺给 QML，界面就会直接
    // 持有底层对象，甚至为了读枚举而不得不认识 C++ 类型名。
    QQmlContext *ctx = m_engine->rootContext();
    ctx->setContextProperty(QStringLiteral("appContext"), m_context);
}
