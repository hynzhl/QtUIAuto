#include <windows.h>
#include <QDebug>
#include <QCoreApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <atomic>
#include "ControlScanner.h"
#include "InputSimulator.h"
#include "CommandHandler.h"

static ControlScanner *g_scanner = nullptr;
static InputSimulator *g_simulator = nullptr;
static CommandHandler *g_handler = nullptr;
static QLocalSocket *g_pipe = nullptr;
static QByteArray g_readBuf;
static std::atomic<bool> g_shutdown = false;
static std::atomic<bool> g_hookInitialized = false;
static std::atomic<bool> g_processing = false;

static void winLog(const char *msg)
{
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    strcat_s(path, "QtUIAuto_Inject.log");
    HANDLE hFile = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    SetFilePointer(hFile, 0, NULL, FILE_END);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[512];
    int len = sprintf_s(buf, "[%02u:%02u:%02u.%03u] %s\r\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
    DWORD written;
    WriteFile(hFile, buf, (DWORD)len, &written, NULL);
    CloseHandle(hFile);
}

// DLL_PROCESS_DETACH 时只置停止标志，不触碰任何 Qt 对象。
// 原因：DETACH 在 loader lock 下执行，disconnectFromServer() / deleteLater()
// 会进入线程与同步原语、可能跨模块调用，属于平台明确禁止的模式，易死锁；
// 而且此时事件循环已随进程消亡，deleteLater() 永远不会被执行——
// 只承担风险、不产生效果。进程正在退出，这些资源由 OS 回收即可。
static void cleanup()
{
    g_shutdown = true;
}

// 处理一条完整命令，并把响应以 NDJSON（单行 JSON + '\n'）写回管道
static void dispatchCommand(const QByteArray &payload)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError)
    {
        qWarning() << "[Inject] JSON parse error:" << err.errorString();
        winLog(QStringLiteral("JSON parse error: %1").arg(err.errorString()).toUtf8().constData());
        return;
    }
    if (!doc.isObject()) { qWarning() << "[Inject] JSON not object"; winLog("JSON not object"); return; }

    QJsonObject cmd = doc.object();
    const QString action = cmd.value("action").toString();
    winLog(QStringLiteral("CMD  <- %1").arg(action).toUtf8().constData());

    QJsonObject response = g_handler->handleCommand(cmd);
    QByteArray respData = QJsonDocument(response).toJson(QJsonDocument::Compact);
    respData.append('\n');

    const qint64 written = g_pipe->write(respData);
    // flush() 内部是 waitForWrite(0)，超时 0ms 即返回，本身已是非阻塞。
    // 它返回 false 只表示异步写未在 0ms 内排空（大响应必然如此），
    // 不代表失败，故不据其返回值判定成败，仅用于催动异步写。
    g_pipe->flush();
    winLog(QStringLiteral("RESP -> %1 status=%2 bytes=%3/%4 pending=%5")
               .arg(action)
               .arg(response.value("status").toString())
               .arg(written)
               .arg(respData.size())
               .arg(g_pipe->bytesToWrite())
               .toUtf8().constData());

    // 真故障：write 返回 -1 或短写，此时响应必然不完整，需要显式告警
    if (written != respData.size())
    {
        winLog(QStringLiteral("RESP !! %1 write %2 err=%3 state=%4")
                   .arg(action)
                   .arg(written < 0 ? QStringLiteral("failed") : QStringLiteral("short"))
                   .arg(g_pipe->errorString())
                   .arg(static_cast<int>(g_pipe->state()))
                   .toUtf8().constData());
        qWarning() << "[Inject] 响应写入不完整 -" << action
                   << written << "/" << respData.size() << g_pipe->errorString();
    }
}

static void processPipeData()
{
    if (g_shutdown || !g_pipe || !g_handler) return;
    g_readBuf.append(g_pipe->readAll());
    if (g_readBuf.isEmpty()) return;

    // 重入保护：命令执行期间若嵌套触发 readyRead，只收数据，由外层循环继续消费
    if (g_processing) return;
    g_processing = true;

    for (;;)
    {
        const int idx = g_readBuf.indexOf('\n');
        if (idx >= 0)
        {
            QByteArray line = g_readBuf.left(idx).trimmed();
            g_readBuf.remove(0, idx + 1);
            if (!line.isEmpty()) dispatchCommand(line);
            continue;
        }

        // 兼容不带换行结尾的命令：残余数据本身已是完整 JSON 对象时立即处理
        if (g_readBuf.isEmpty()) break;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(g_readBuf, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) break;
        QByteArray payload = g_readBuf;
        g_readBuf.clear();
        dispatchCommand(payload);
    }

    g_processing = false;
}

static void connectPipe()
{
    if (g_shutdown) return;

    DWORD pid = GetCurrentProcessId();
    char pipeName[64];
    sprintf_s(pipeName, "QtUIAuto_%lu", pid);
    winLog(pipeName);
    QString qPipeName = QString::fromLatin1(pipeName);

    g_pipe = new QLocalSocket();
    QObject::connect(g_pipe, &QLocalSocket::readyRead, processPipeData);
    QObject::connect(g_pipe, static_cast<void (QLocalSocket::*)(QLocalSocket::LocalSocketError)>(&QLocalSocket::error), [](QLocalSocket::LocalSocketError err) { qWarning() << "[Inject] Pipe error:" << err; });
    g_pipe->connectToServer(qPipeName, QIODevice::ReadWrite);
    if (g_pipe->waitForConnected(3000))
    {
        winLog("Pipe connected OK");
        QJsonObject hello;
        hello["action"] = QStringLiteral("inject_ready");
        QByteArray helloData = QJsonDocument(hello).toJson(QJsonDocument::Compact);
        helloData.append('\n');
        g_pipe->write(helloData);
        g_pipe->flush();
        winLog("inject_ready sent");
    }
    else
    {
        winLog("Pipe connect FAILED");
        qWarning() << "[Inject] Pipe connect failed:" << qPipeName;
    }
}

static void initInMainThread()
{
    winLog("initInMainThread start");
    g_scanner = new ControlScanner();
    g_simulator = new InputSimulator();
    g_handler = new CommandHandler(g_scanner, g_simulator);

    // Defer connection so the caller (e.g., hook proc) returns immediately.
    // The actual connection runs on the target's main thread event loop.
    QTimer::singleShot(0, g_scanner, &connectPipe);
}

extern "C" __declspec(dllexport) LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && !g_hookInitialized.exchange(true))
    {
        winLog("GetMsgProc first call, initializing agent");
        initInMainThread();
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_shutdown = false;
        DisableThreadLibraryCalls(hModule);
        winLog("DLL_PROCESS_ATTACH");
        // Hook-based injection: GetMsgProc triggers initInMainThread on the
        // target's main thread when the first message is processed.
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        winLog("DLL_PROCESS_DETACH");
        cleanup();
    }
    return TRUE;
}
