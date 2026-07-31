// 宿主端链路测试：不经 GUI，直接驱动真实的 ProcessManager / PipeServer / ScriptEngine，
// 覆盖 E2E 测不到的那一半——E2E 自己实现了一套管道客户端，绕过了这三个类。
//
// 各用例对应的缺陷：
//   NDJSON 组帧      → PipeServer 写命令未追加 '\n'，粘包会让对端缓冲区永久滞留
//   flush 语义       → 以 flush() 返回值判定写入失败（Windows 上等价 waitForWrite(0)）
//   超时响应错位     → 超时命令的迟到响应被下一条命令的事件循环抢走
//   注入线程与时机   → 钩子装到非 GUI 线程 / 窗口未就绪即注入 / 钩子句柄未保存
//   回放阻塞         → startPlayback() 同步跑完全部步骤，GUI 冻结且无法中止

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <windows.h>
#include <cstdio>

#include "app/ProcessManager.h"
#include "engine/PipeServer.h"
#include "engine/ScriptEngine.h"

// ═══════════════════════ 日志 ═══════════════════════

static const char *logPath()
{
    static QByteArray path;
    if (path.isEmpty())
        path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()
                                        + QStringLiteral("/host_chain_diag.log")).toLocal8Bit();
    return path.constData();
}

static void hcLog(const QString &msg)
{
    FILE *f = fopen(logPath(), "a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            msg.toUtf8().constData());
    fclose(f);
}

static int g_pass = 0;
static int g_fail = 0;

static void ok(const QString &name, const QString &detail = QString())
{
    ++g_pass;
    hcLog(name + QStringLiteral(" PASS") + (detail.isEmpty() ? QString() : QStringLiteral(" (") + detail + QStringLiteral(")")));
}

static void bad(const QString &name, const QString &reason)
{
    ++g_fail;
    hcLog(name + QStringLiteral(" FAIL: ") + reason);
}

// 没有 skip() 是有意为之：一旦允许跳过，前置条件靠时序碰运气的用例就会静默失效，
// 而日志里又看不到 FAIL。每个用例都必须把前置条件构造出来，造不出来就计 FAIL。
static void check(const QString &name, bool condition, const QString &detail)
{
    if (condition) ok(name, detail);
    else bad(name, detail);
}

// ═══════════════════════ 辅助 ═══════════════════════

static QString findFile(const QString &name)
{
    const QString dir = QCoreApplication::applicationDirPath();
    QString path = dir + QStringLiteral("/") + name;
    if (QFileInfo::exists(path)) return QFileInfo(path).absoluteFilePath();
    path = dir + QStringLiteral("/../") + name;
    if (QFileInfo::exists(path)) return QFileInfo(path).absoluteFilePath();
    return QString();
}

// 等待某个信号，返回是否在超时前等到。
// sender 必须一并模版化：connect 要求 sender 类型与信号所属类一致，
// 声明成 QObject * 会导致重载解析失败。
template <typename Sender, typename Signal>
static bool waitForSignal(Sender *sender, Signal signal, int timeoutMs)
{
    QEventLoop loop;
    bool fired = false;
    auto conn = QObject::connect(sender, signal, &loop, [&]() {
        fired = true;
        loop.quit();
    });
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    QObject::disconnect(conn);
    return fired;
}

static void spin(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static QJsonObject step(const QString &action, const QString &target)
{
    QJsonObject o;
    o[QStringLiteral("action")] = action;
    o[QStringLiteral("findBy")] = QStringLiteral("objectName");
    o[QStringLiteral("target")] = target;
    return o;
}

// ═══════════════════════ 用例 ═══════════════════════

// 单命令往返打通，说明 NDJSON 组帧与 write/flush 判据都正确
static void testSingleCommand(PipeServer *pipe)
{
    QJsonObject cmd;
    cmd[QStringLiteral("action")] = QStringLiteral("dumptree");
    const QJsonObject resp = pipe->sendCommand(cmd, 10000);

    if (resp.value(QStringLiteral("status")).toString() != QStringLiteral("ok"))
    {
        bad(QStringLiteral("sendCommand/dumptree"),
            QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact)));
        return;
    }

    const QJsonObject tree = resp.value(QStringLiteral("data")).toObject()
                                 .value(QStringLiteral("tree")).toObject();
    check(QStringLiteral("sendCommand/dumptree"), !tree.isEmpty(),
          QStringLiteral("tree keys=%1").arg(tree.keys().size()));
}

// 连发多条：验证对端缓冲区不会被上一帧污染，且每条响应都能对上自己的请求
static void testSequentialCommands(PipeServer *pipe)
{
    const QStringList actions = { QStringLiteral("ping"), QStringLiteral("listcontrols"),
                                  QStringLiteral("ping"), QStringLiteral("dumptree"),
                                  QStringLiteral("ping") };
    int matched = 0;
    for (const QString &action : actions)
    {
        QJsonObject cmd;
        cmd[QStringLiteral("action")] = action;
        const QJsonObject resp = pipe->sendCommand(cmd, 10000);
        if (resp.value(QStringLiteral("status")).toString() != QStringLiteral("ok"))
        {
            bad(QStringLiteral("sequentialCommands"),
                QStringLiteral("%1 未返回 ok: %2").arg(action,
                    QString::fromUtf8(QJsonDocument(resp).toJson(QJsonDocument::Compact))));
            return;
        }
        // 响应里的 action 必须与请求一致，否则就是响应错位
        if (resp.value(QStringLiteral("action")).toString() != action)
        {
            bad(QStringLiteral("sequentialCommands"),
                QStringLiteral("响应错位: 请求=%1 响应=%2")
                    .arg(action, resp.value(QStringLiteral("action")).toString()));
            return;
        }
        ++matched;
    }
    ok(QStringLiteral("sequentialCommands"), QStringLiteral("%1 条命令全部对齐").arg(matched));
}

// 核心用例：命令超时后，其迟到响应不得被下一条命令认领
static void testTimeoutNoMisalignment(PipeServer *pipe)
{
    // 超时必须靠构造而非赛跑：Windows 上 Qt 定时器基于 SetTimer，最小粒度约 15ms，
    // 给个 1ms 反而会等到 ~15ms 后才触发，而本地管道往返仅 1ms 左右，超时永远造不出来。
    // timeoutMs=0 的零定时器在事件循环首轮即触发，此时目标进程尚未被调度、
    // 响应不可能到达，超时是必然结果。
    QJsonObject slow;
    slow[QStringLiteral("action")] = QStringLiteral("dumptree");
    const QJsonObject slowResp = pipe->sendCommand(slow, 0);

    if (slowResp.value(QStringLiteral("status")).toString() == QStringLiteral("ok"))
    {
        bad(QStringLiteral("timeoutNoMisalignment"),
            QStringLiteral("零超时仍返回 ok，无法构造超时场景，用例失去意义"));
        return;
    }

    // 不能在这里 spin 等迟到响应：那样它会在无人监听 responseReceived 时到达，
    // 被无害丢掉，错位根本不会发生，用例就成了恒真。必须紧跟下一条命令，
    // 让迟到响应撞进它的事件循环——这才是真实的错位现场。
    QJsonObject ping;
    ping[QStringLiteral("action")] = QStringLiteral("ping");
    const QJsonObject pingResp = pipe->sendCommand(ping, 5000);

    const QString status = pingResp.value(QStringLiteral("status")).toString();
    const QString action = pingResp.value(QStringLiteral("action")).toString();

    if (status != QStringLiteral("ok"))
    {
        bad(QStringLiteral("timeoutNoMisalignment"),
            QStringLiteral("超时后续命令失败: %1")
                .arg(QString::fromUtf8(QJsonDocument(pingResp).toJson(QJsonDocument::Compact))));
        return;
    }
    // 修复前这里会拿到迟到的 dumptree 响应
    check(QStringLiteral("timeoutNoMisalignment"), action == QStringLiteral("ping"),
          QStringLiteral("后续命令响应 action=%1").arg(action));
}

// 回放必须立即返回（不阻塞调用方），并通过信号异步汇报进度
static void testPlaybackNonBlocking(ScriptEngine *script)
{
    QVariantList steps;
    steps << step(QStringLiteral("click"), QStringLiteral("btnClickMe")).toVariantMap()
          << step(QStringLiteral("getText"), QStringLiteral("clickResultLabel")).toVariantMap()
          << step(QStringLiteral("click"), QStringLiteral("btnClickMe")).toVariantMap();
    script->loadScriptFromJson(steps);

    int lastStep = 0;
    int lastTotal = 0;
    auto stepConn = QObject::connect(script, &ScriptEngine::playbackStep,
                                     [&](int s, int t) { lastStep = s; lastTotal = t; });

    bool finished = false;
    bool success = false;
    auto finConn = QObject::connect(script, &ScriptEngine::playbackFinished,
                                    [&](bool ok_) { finished = true; success = ok_; });

    QElapsedTimer timer;
    timer.start();
    script->startPlayback();
    const qint64 returnedAfter = timer.elapsed();

    // 修复前 startPlayback() 会同步跑完所有步骤（含 msleep），耗时远超此阈值
    check(QStringLiteral("playback/nonBlockingReturn"), returnedAfter < 100,
          QStringLiteral("startPlayback 返回耗时 %1ms").arg(returnedAfter));

    const bool got = waitForSignal(script, &ScriptEngine::playbackFinished, 15000);
    if (!got && !finished)
    {
        bad(QStringLiteral("playback/finished"), QStringLiteral("未收到 playbackFinished"));
    }
    else
    {
        check(QStringLiteral("playback/finished"), success,
              QStringLiteral("success=%1 进度=%2/%3").arg(success).arg(lastStep).arg(lastTotal));
        check(QStringLiteral("playback/allStepsRan"), lastStep == steps.size() && lastTotal == steps.size(),
              QStringLiteral("最后进度 %1/%2, 期望 %3").arg(lastStep).arg(lastTotal).arg(steps.size()));
    }

    QObject::disconnect(stepConn);
    QObject::disconnect(finConn);
}

// 回放期间事件循环保持可响应，stopPlayback() 才能真正中止
static void testPlaybackStoppable(ScriptEngine *script)
{
    QVariantList steps;
    for (int i = 0; i < 8; ++i)
        steps << step(QStringLiteral("click"), QStringLiteral("btnClickMe")).toVariantMap();
    script->loadScriptFromJson(steps);

    int lastStep = 0;
    auto stepConn = QObject::connect(script, &ScriptEngine::playbackStep,
                                     [&](int s, int) { lastStep = s; });
    bool finished = false;
    bool success = true;
    auto finConn = QObject::connect(script, &ScriptEngine::playbackFinished,
                                    [&](bool ok_) { finished = true; success = ok_; });

    script->startPlayback();
    script->stopPlayback();     // 修复前 startPlayback 已同步跑完，这里根本来不及调用

    check(QStringLiteral("playback/stopEmitsFinished"), finished && !success,
          QStringLiteral("finished=%1 success=%2").arg(finished).arg(success));

    // 已投递的 runNextStep 应在状态检查处静默退出，不得继续推进步骤
    const int stepAtStop = lastStep;
    spin(800);
    check(QStringLiteral("playback/stopHaltsSteps"), lastStep == stepAtStop,
          QStringLiteral("中止时 step=%1, 800ms 后 step=%2").arg(stepAtStop).arg(lastStep));
    check(QStringLiteral("playback/stopBeforeAllSteps"), lastStep < steps.size(),
          QStringLiteral("已执行 %1 步, 总 %2 步").arg(lastStep).arg(steps.size()));

    QObject::disconnect(stepConn);
    QObject::disconnect(finConn);
}

// ═══════════════════════ 主流程 ═══════════════════════

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    remove(logPath());
    hcLog(QStringLiteral("===== Host Chain Test ====="));

    const QString targetPath = findFile(QStringLiteral("QtUIAuto_TestTarget.exe"));
    if (targetPath.isEmpty())
    {
        hcLog(QStringLiteral("FATAL: 找不到 QtUIAuto_TestTarget.exe"));
        return 1;
    }
    const QString dllPath = findFile(QStringLiteral("QtUIAuto_Inject.dll"));
    if (dllPath.isEmpty())
    {
        hcLog(QStringLiteral("FATAL: 找不到 QtUIAuto_Inject.dll"));
        return 1;
    }
    hcLog(QStringLiteral("Target: ") + targetPath);
    hcLog(QStringLiteral("DLL: ") + dllPath);

    ProcessManager process;
    PipeServer pipe;
    ScriptEngine script(&pipe);
    process.setInjectDllPath(dllPath);

    QObject::connect(&process, &ProcessManager::injectionResult,
                     [](bool success, const QString &message) {
        hcLog(QStringLiteral("injectionResult: success=%1 message=%2")
                  .arg(success).arg(message));
    });

    // 复现 Application 的编排：启动 → 起管道 → 等窗口就绪 → 注入
    if (!process.launchTarget(targetPath))
    {
        bad(QStringLiteral("launchTarget"), QStringLiteral("目标进程启动失败"));
        hcLog(QStringLiteral("Done. Pass: %1 Fail: %2").arg(g_pass).arg(g_fail));
        return 1;
    }
    ok(QStringLiteral("launchTarget"), QStringLiteral("PID=%1").arg(process.targetPid()));

    if (!pipe.start(process.targetPid()))
    {
        bad(QStringLiteral("pipeServer/start"), QStringLiteral("管道监听失败"));
        process.stopTarget();
        hcLog(QStringLiteral("Done. Pass: %1 Fail: %2").arg(g_pass).arg(g_fail));
        return 1;
    }
    ok(QStringLiteral("pipeServer/start"), QStringLiteral("监听 QtUIAuto_%1").arg(process.targetPid()));

    // 与 Application 中 kInjectDelayMs 一致：窗口与 QML 引擎需要时间创建
    spin(1500);

    if (!process.injectDll())
    {
        bad(QStringLiteral("injectDll"), QStringLiteral("注入调用返回 false"));
        process.stopTarget();
        hcLog(QStringLiteral("Done. Pass: %1 Fail: %2").arg(g_pass).arg(g_fail));
        return 1;
    }
    ok(QStringLiteral("injectDll"), QStringLiteral("注入已触发"));

    if (!waitForSignal(&pipe, &PipeServer::injectReady, 15000))
    {
        bad(QStringLiteral("injectReady"), QStringLiteral("15s 内未收到 inject_ready"));
        process.stopTarget();
        hcLog(QStringLiteral("Done. Pass: %1 Fail: %2").arg(g_pass).arg(g_fail));
        return 1;
    }
    ok(QStringLiteral("injectReady"), QStringLiteral("注入侧已握手"));
    check(QStringLiteral("pipeServer/connected"), pipe.isConnected(), QStringLiteral("connected=%1").arg(pipe.isConnected()));

    testSingleCommand(&pipe);
    testSequentialCommands(&pipe);
    testPlaybackNonBlocking(&script);
    testPlaybackStoppable(&script);
    testTimeoutNoMisalignment(&pipe);   // 会制造一次超时，放在最后避免干扰其它用例

    // stopTarget 内部会 releaseHook：卸载钩子并释放本地 DLL 引用计数
    process.stopTarget();
    check(QStringLiteral("stopTarget"), !process.isRunning(),
          QStringLiteral("running=%1").arg(process.isRunning()));

    pipe.stop();

    hcLog(QStringLiteral("Done. Pass: %1 Fail: %2").arg(g_pass).arg(g_fail));
    return g_fail > 0 ? 1 : 0;
}
