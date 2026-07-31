#include <QCoreApplication>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <QDir>
#include <QThread>
#include <windows.h>
#include <TlHelp32.h>
#include <cstdio>

// 日志与可执行文件同目录：不能硬编码绝对路径，否则换机器 / 换盘符 / CI 上
// fopen 会静默失败，诊断信息直接丢失。首次调用时解析并缓存。
static const char *e2eLogPath()
{
    static QByteArray path;
    if (path.isEmpty())
        path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()
                                        + QStringLiteral("/e2e_diag.log")).toLocal8Bit();
    return path.constData();
}

static void e2eLog(const char *msg)
{
    FILE *f = fopen(e2eLogPath(), "a");
    if (f) { SYSTEMTIME st; GetLocalTime(&st); fprintf(f, "[%02u:%02u:%02u.%03u] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg); fclose(f); }
}

static void enableDebugPrivilege()
{
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return;
    LUID luid;
    if (LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid))
    {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(hToken);
}
static void logAdminStatus()
{
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) { e2eLog("OpenProcessToken failed"); return; }
    TOKEN_ELEVATION elevation;
    DWORD retLen = 0;
    if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &retLen))
    {
        e2eLog(elevation.TokenIsElevated ? "Running elevated: YES" : "Running elevated: NO");
    }
    else
    {
        char errBuf[128]; sprintf_s(errBuf, "GetTokenInformation failed: %lu", GetLastError()); e2eLog(errBuf);
    }
    CloseHandle(hToken);
}
static void logMitigations(HANDLE hProcess)
{
    PROCESS_MITIGATION_POLICY policies[] = { ProcessDEPPolicy, ProcessASLRPolicy, ProcessDynamicCodePolicy, ProcessStrictHandleCheckPolicy, ProcessSystemCallDisablePolicy, ProcessMitigationOptionsMask, ProcessExtensionPointDisablePolicy, ProcessControlFlowGuardPolicy, ProcessSignaturePolicy, ProcessFontDisablePolicy, ProcessImageLoadPolicy, ProcessSystemCallFilterPolicy, ProcessPayloadRestrictionPolicy, ProcessChildProcessPolicy, ProcessSideChannelIsolationPolicy, ProcessUserShadowStackPolicy, ProcessRedirectionTrustPolicy };
    const char *names[] = { "DEP", "ASLR", "DynamicCode", "StrictHandle", "SysCallDisable", "MitigationOptionsMask", "ExtensionPointDisable", "CFG", "Signature", "FontDisable", "ImageLoad", "SysCallFilter", "PayloadRestriction", "ChildProcess", "SideChannelIsolation", "UserShadowStack", "RedirectionTrust" };
    e2eLog("Target process mitigation policies:");
    for (int i = 0; i < 17; ++i)
    {
        DWORD value = 0;
        if (GetProcessMitigationPolicy(hProcess, policies[i], &value, sizeof(value)))
        {
            char buf[128]; sprintf_s(buf, "Mitigation %s: 0x%08X", names[i], value); e2eLog(buf);
        }
        else
        {
            char buf[128]; sprintf_s(buf, "Mitigation %s: query failed err=%lu", names[i], GetLastError()); e2eLog(buf);
        }
    }
}

static bool probeRemoteMemory(HANDLE hProcess)
{
    MEMORY_BASIC_INFORMATION mbi = {};
    uintptr_t addr = 0x10000;
    while (addr < 0x7FFF00000000ULL)
    {
        SIZE_T ret = VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi));
        if (!ret)
            break;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        {
            unsigned char readBuf[1] = {};
            SIZE_T readBytes = 0;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, readBuf, 1, &readBytes))
            {
                e2eLog("ReadProcessMemory: OK");
                return true;
            }
            else
            {
                char errBuf[128]; sprintf_s(errBuf, "ReadProcessMemory failed: %lu", GetLastError()); e2eLog(errBuf);
                return false;
            }
        }
        addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    e2eLog("ReadProcessMemory: no committed readable region found");
    return false;
}

static void logSystemError(const char *prefix, DWORD err)
{
    LPSTR msgBuf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);
    char buf[512];
    sprintf_s(buf, "%s: error=%lu, msg=%s", prefix, err, msgBuf ? msgBuf : "unknown");
    if (msgBuf) LocalFree(msgBuf);
    e2eLog(buf);
}
static void logProcessIntegrity(HANDLE hProcess, const char *name)
{
    HANDLE hToken;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) { e2eLog("OpenProcessToken(target) failed"); return; }
    DWORD len = 0;
    GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &len);
    if (len > 0)
    {
        TOKEN_MANDATORY_LABEL *tml = (TOKEN_MANDATORY_LABEL*)malloc(len);
        if (GetTokenInformation(hToken, TokenIntegrityLevel, tml, len, &len))
        {
            DWORD rid = *GetSidSubAuthority(tml->Label.Sid, *GetSidSubAuthorityCount(tml->Label.Sid) - 1);
            char buf[128]; sprintf_s(buf, "%s integrity level: %lu", name, rid); e2eLog(buf);
        }
        free(tml);
    }
    CloseHandle(hToken);
}
static QLocalServer *g_server = nullptr;
static QLocalSocket *g_client = nullptr;
static QProcess *g_target = nullptr;
static QString g_pipeName;
static QByteArray g_recvBuf;
static QMetaObject::Connection g_readyConn;
static HHOOK g_hook = nullptr;
static HMODULE g_hookDll = nullptr;
static int g_testPass = 0;
static int g_testFail = 0;
static bool g_ready = false;

static QString findFile(const QString &name)
{
    QString buildDir = QCoreApplication::applicationDirPath();
    QString path = buildDir + "/" + name;
    if (QFileInfo::exists(path)) return QFileInfo(path).absoluteFilePath();
    if (name.contains("TestTarget")) { QString alt = buildDir + "/../../test-target/Release/" + name; if (QFileInfo::exists(alt)) return QFileInfo(alt).absoluteFilePath(); }
    QString alt2 = buildDir + "/../" + name;
    if (QFileInfo::exists(alt2)) return QFileInfo(alt2).absoluteFilePath();
    return QString();
}

static DWORD getMainThreadId(DWORD processId)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        char errBuf[128]; sprintf_s(errBuf, "CreateToolhelp32Snapshot failed: %lu", GetLastError()); e2eLog(errBuf);
        return 0;
    }

    DWORD mainThreadId = 0;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    if (Thread32First(hSnapshot, &te))
    {
        do {
            if (te.th32OwnerProcessID == processId)
            {
                mainThreadId = te.th32ThreadID;
                break;
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);
    return mainThreadId;
}

static DWORD getWindowThreadId(DWORD processId)
{
    HWND hwnd = FindWindowW(nullptr, L"QtUIAuto Test Target");
    if (!hwnd) return 0;
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    if (pid != processId) return 0;
    return tid;
}

static bool injectViaHook(quint64 targetPid, const QString &dllPath)
{
    const DWORD pid = static_cast<DWORD>(targetPid);
    DWORD tid = getWindowThreadId(pid);
    if (tid == 0)
    {
        e2eLog("Window thread not found, falling back to thread snapshot");
        tid = getMainThreadId(pid);
    }
    if (tid == 0) { e2eLog("FAIL: cannot get main thread id"); return false; }
    char tidBuf[128]; sprintf_s(tidBuf, "Target main thread id: %lu", tid); e2eLog(tidBuf);

    std::wstring wDllPath = dllPath.toStdWString();
    HMODULE hLocalDll = LoadLibraryW(wDllPath.c_str());
    if (!hLocalDll) { e2eLog("FAIL: LoadLibraryW local failed"); return false; }

    HOOKPROC hookProc = reinterpret_cast<HOOKPROC>(GetProcAddress(hLocalDll, "GetMsgProc"));
    if (!hookProc) { e2eLog("FAIL: GetMsgProc not found in DLL"); FreeLibrary(hLocalDll); return false; }

    HHOOK hHook = SetWindowsHookEx(WH_GETMESSAGE, hookProc, hLocalDll, tid);
    if (!hHook) { e2eLog("FAIL: SetWindowsHookEx failed"); FreeLibrary(hLocalDll); return false; }

    if (!PostThreadMessage(tid, WM_NULL, 0, 0)) { e2eLog("FAIL: PostThreadMessage failed"); UnhookWindowsHookEx(hHook); FreeLibrary(hLocalDll); return false; }

    // 保存句柄，用于测试结束后卸载钩子
    g_hook = hHook;
    g_hookDll = hLocalDll;

    char buf[128]; sprintf_s(buf, "Hook injected, tid=%lu", tid); e2eLog(buf);
    return true;
}

static void releaseHook()
{
    if (g_hook) { UnhookWindowsHookEx(g_hook); g_hook = nullptr; e2eLog("Hook released"); }
    if (g_hookDll) { FreeLibrary(g_hookDll); g_hookDll = nullptr; }
}

// 按 NDJSON 组帧发送单条命令并等待完整的一行响应。
// 必须在事件循环顶层调用（不得在 g_client 的 readyRead 处理器内部调用）。
static QJsonObject sendCommand(const QJsonObject &cmd, int timeoutMs = 5000)
{
    QJsonObject errorResp; errorResp["status"] = "error"; errorResp["message"] = "not connected";
    if (!g_client) return errorResp;

    QByteArray data = QJsonDocument(cmd).toJson(QJsonDocument::Compact);
    data.append('\n');
    g_client->write(data);
    if (!g_client->waitForBytesWritten(1000)) { errorResp["message"] = "write timeout"; return errorResp; }

    // 按行累积读取，避免大响应分片时拿到截断的 JSON
    QElapsedTimer timer;
    timer.start();
    g_recvBuf.append(g_client->readAll());
    int newlinePos = g_recvBuf.indexOf('\n');
    while (newlinePos < 0)
    {
        const qint64 remain = timeoutMs - timer.elapsed();
        if (remain <= 0)
        {
            errorResp["message"] = QString("response timeout, buffered=%1 bytes").arg(g_recvBuf.size());
            return errorResp;
        }
        if (g_client->state() != QLocalSocket::ConnectedState)
        {
            errorResp["message"] = QString("socket not connected, state=%1").arg(static_cast<int>(g_client->state()));
            return errorResp;
        }
        if (g_client->waitForReadyRead(static_cast<int>(remain)))
            g_recvBuf.append(g_client->readAll());
        newlinePos = g_recvBuf.indexOf('\n');
    }

    QByteArray respData = g_recvBuf.left(newlinePos).trimmed();
    g_recvBuf.remove(0, newlinePos + 1);

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(respData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        errorResp["message"] = QString("JSON failed: ") + err.errorString() + " raw=" + QString::fromUtf8(respData.left(512));
        return errorResp;
    }
    return doc.object();
}

// 取出响应里的降级轨道名，拼成 " [Window::sendEvent]" 这样的后缀
static QString methodSuffix(const QJsonObject &resp)
{
    const QString method = resp.value("data").toObject().value("method").toString();
    return method.isEmpty() ? QString() : QStringLiteral(" [") + method + QStringLiteral("]");
}

static void checkResult(const QString &testName, const QJsonObject &resp, const QString &expectedStatus = "ok")
{
    QString status = resp.value("status").toString();
    if (status == expectedStatus)
    {
        g_testPass++;
        e2eLog((testName + " PASS" + methodSuffix(resp)).toUtf8().constData());
    }
    else
    {
        g_testFail++;
        QByteArray raw = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        e2eLog((testName + " FAIL: " + QString::fromUtf8(raw)).toUtf8().constData());
    }
}

// 除了校验 status，还校验返回的文本内容，避免“命令成功但 UI 未生效”被误判为通过
static void checkText(const QString &testName, const QJsonObject &resp, const QString &expected)
{
    const QString status = resp.value("status").toString();
    if (status != QStringLiteral("ok"))
    {
        g_testFail++;
        QByteArray raw = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        e2eLog((testName + " FAIL: " + QString::fromUtf8(raw)).toUtf8().constData());
        return;
    }

    const QString actual = resp.value("data").toObject().value("text").toString();
    if (actual == expected)
    {
        g_testPass++;
        e2eLog((testName + " PASS (text=\"" + actual + "\")" + methodSuffix(resp)).toUtf8().constData());
    }
    else
    {
        g_testFail++;
        e2eLog((testName + " FAIL: 文本不符, 期望=\"" + expected + "\" 实际=\"" + actual + "\"" + methodSuffix(resp)).toUtf8().constData());
    }
}

// 校验 data[key] 确为非空数组。仅判 status 无法发现“对象 / 数组”结构回归，
// 而结构不匹配在主程序侧只会表现为“列表空”，没有任何错误提示
static void checkArrayNonEmpty(const QString &testName, const QJsonObject &resp, const QString &key)
{
    const QString status = resp.value("status").toString();
    if (status != QStringLiteral("ok"))
    {
        g_testFail++;
        QByteArray raw = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        e2eLog((testName + " FAIL: " + QString::fromUtf8(raw)).toUtf8().constData());
        return;
    }

    const QJsonValue value = resp.value("data").toObject().value(key);
    if (!value.isArray())
    {
        g_testFail++;
        e2eLog((testName + " FAIL: data." + key + " 不是数组").toUtf8().constData());
        return;
    }

    const QJsonArray arr = value.toArray();
    if (arr.isEmpty())
    {
        g_testFail++;
        e2eLog((testName + " FAIL: data." + key + " 为空数组").toUtf8().constData());
        return;
    }

    g_testPass++;
    e2eLog((testName + " PASS (" + key + " count=" + QString::number(arr.size()) + ")").toUtf8().constData());
}

static void runTests()
{
    e2eLog("===== E2E Tests =====");

    // 先跑 dumpTree：它产生大响应，用于验证分片组帧是否正确
    QJsonObject dumpCmd; dumpCmd["action"] = "dumptree";
    checkResult("dumpTree", sendCommand(dumpCmd, 10000));

    // listcontrols 必须返回数组：主程序侧 ControlTree 按数组解析，
    // 若退化为对象则恒得空列表而 status 仍为 ok，只能用类型断言拦住
    QJsonObject listCmd; listCmd["action"] = "listcontrols";
    checkArrayNonEmpty("listControls", sendCommand(listCmd, 10000), QStringLiteral("controls"));

    QJsonObject clickCmd; clickCmd["action"] = "click"; clickCmd["findBy"] = "objectName"; clickCmd["target"] = "btnClickMe";
    checkResult("click btnClickMe", sendCommand(clickCmd)); QThread::msleep(200);

    QJsonObject getTextCmd; getTextCmd["action"] = "getText"; getTextCmd["findBy"] = "objectName"; getTextCmd["target"] = "clickResultLabel";
    checkText("getText clickResultLabel", sendCommand(getTextCmd), QStringLiteral("按钮被点击了 1 次"));

    QJsonObject typeCmd; typeCmd["action"] = "typeText"; typeCmd["findBy"] = "objectName"; typeCmd["target"] = "textInput"; typeCmd["text"] = "Hello World";
    checkResult("typeText", sendCommand(typeCmd)); QThread::msleep(200);

    QJsonObject inputCmd; inputCmd["action"] = "getText"; inputCmd["findBy"] = "objectName"; inputCmd["target"] = "textInput";
    checkText("getText textInput", sendCommand(inputCmd), QStringLiteral("Hello World"));

    QJsonObject showCmd; showCmd["action"] = "click"; showCmd["findBy"] = "objectName"; showCmd["target"] = "btnShowText";
    checkResult("click btnShowText", sendCommand(showCmd)); QThread::msleep(200);

    QJsonObject verifyCmd; verifyCmd["action"] = "getText"; verifyCmd["findBy"] = "objectName"; verifyCmd["target"] = "textDisplayLabel";
    checkText("getText textDisplayLabel", sendCommand(verifyCmd), QStringLiteral("输入内容: Hello World"));

    QJsonObject dblCmd; dblCmd["action"] = "doubleclick"; dblCmd["findBy"] = "objectName"; dblCmd["target"] = "btnClickMe";
    checkResult("doubleClick", sendCommand(dblCmd));

    QJsonObject focusCmd; focusCmd["action"] = "setFocus"; focusCmd["findBy"] = "objectName"; focusCmd["target"] = "textInput";
    checkResult("setFocus", sendCommand(focusCmd));

    e2eLog("===== E2E Tests Done =====");
}


static void onInjectReady()
{
    e2eLog("Inject DLL ready");
    g_ready = true;
    runTests();
    QCoreApplication::quit();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // 先清日志再写第一行，否则首行会被 remove 掉；
    // 路径依赖 applicationDirPath()，故必须在 QCoreApplication 构造之后取
    remove(e2eLogPath());
    e2eLog("E2E started");
    enableDebugPrivilege();
    logAdminStatus();
    HANDLE hSelf2 = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (hSelf2) { logProcessIntegrity(hSelf2, "Self"); CloseHandle(hSelf2); }
    e2eLog("SeDebugPrivilege enabled");

    QString targetPath = findFile("QtUIAuto_TestTarget.exe");
    if (targetPath.isEmpty()) { e2eLog("FAIL: cannot find TestTarget.exe"); return 1; }
    e2eLog(QString("Target: " + targetPath).toUtf8().constData());

    QString dllPath = findFile("QtUIAuto_Inject.dll");
    if (dllPath.isEmpty()) { e2eLog("FAIL: cannot find Inject.dll"); return 1; }
    e2eLog(QString("DLL: " + dllPath).toUtf8().constData());

    g_target = new QProcess(&app);
    g_target->setProgram(targetPath);
    g_target->start();
    if (!g_target->waitForStarted(5000)) { e2eLog("FAIL: target not started"); return 1; }
    qint64 targetPid = g_target->processId();
    e2eLog(QString("Target PID: " + QString::number(targetPid)).toUtf8().constData());
    QThread::msleep(1500);

    g_pipeName = QStringLiteral("QtUIAuto_%1").arg(targetPid);
    QLocalServer::removeServer(g_pipeName);
    g_server = new QLocalServer(&app);
    QObject::connect(g_server, &QLocalServer::newConnection, []() {
        g_client = g_server->nextPendingConnection();
        if (!g_client) return;
        e2eLog("Agent connected");
        g_readyConn = QObject::connect(g_client, &QLocalSocket::readyRead, []() {
            g_recvBuf.append(g_client->readAll());
            const int idx = g_recvBuf.indexOf('\n');
            if (idx < 0) return;
            QByteArray line = g_recvBuf.left(idx).trimmed();
            QJsonDocument doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) return;
            if (doc.object().value("action").toString() != QStringLiteral("inject_ready")) return;
            g_recvBuf.remove(0, idx + 1);
            e2eLog("inject_ready received");
            // 先断开本处理器并延后到事件循环顶层执行，
            // 避免在 readyRead 内部对同一 socket 做嵌套阻塞读（会偷走响应甚至导致崩溃）
            QObject::disconnect(g_readyConn);
            QTimer::singleShot(0, qApp, &onInjectReady);
        });
    });


    if (!g_server->listen(g_pipeName)) { e2eLog("FAIL: pipe listen failed"); g_target->kill(); return 1; }
    e2eLog(QString("PipeServer listening: " + g_pipeName).toUtf8().constData());

    if (!injectViaHook(targetPid, dllPath)) { e2eLog("FAIL: hook injection failed"); g_target->kill(); return 1; }

    e2eLog("Waiting for inject_ready...");
    QTimer::singleShot(15000, [&]() {
        if (!g_ready) { e2eLog("FAIL: timeout waiting for inject_ready"); g_target->kill(); app.quit(); }
    });

    int ret = app.exec();
    releaseHook();
    if (g_target && g_target->state() != QProcess::NotRunning)
    {
        g_target->terminate();
        if (!g_target->waitForFinished(3000))
            g_target->kill();
        g_target->waitForFinished(1000);
    }
    // g_target is a child of app; do not manually delete it here.
    char summary[128]; sprintf_s(summary, "Done. Pass: %d Fail: %d", g_testPass, g_testFail); e2eLog(summary);
    return (g_testFail > 0) ? 1 : 0;
}
