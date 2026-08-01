#include "ProcessManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <memory>
#include <windows.h>
#include <TlHelp32.h>

// 快照里的第一个线程并不保证是 GUI 线程，仅作为拿不到窗口时的回退
static DWORD getMainThreadId(DWORD processId)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

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

struct WindowThreadSearch
{
    DWORD processId  = 0;
    DWORD threadId   = 0;
};

/// RAII wrapper for WH_GETMESSAGE hook handle and the locally loaded DLL module.
/// Destroying it uninstalls the hook and releases the library reference count.
struct ProcessManager::HookHandle
{
    HHOOK hook = nullptr;
    HMODULE module = nullptr;

    HookHandle() = default;
    HookHandle(HHOOK h, HMODULE m)
        : hook(h)
        , module(m)
    {
    }

    ~HookHandle()
    {
        release();
    }

    HookHandle(const HookHandle &) = delete;
    HookHandle &operator=(const HookHandle &) = delete;
    HookHandle(HookHandle &&) = default;
    HookHandle &operator=(HookHandle &&) = default;

    void release()
    {
        if (hook)
        {
            UnhookWindowsHookEx(hook);
            hook = nullptr;
        }
        if (module)
        {
            FreeLibrary(module);
            module = nullptr;
        }
    }
};

void ProcessManager::HookHandleDeleter::operator()(HookHandle *ptr) const
{
    delete ptr;
}

static BOOL CALLBACK enumWindowProc(HWND hwnd, LPARAM lParam)
{
    auto *search = reinterpret_cast<WindowThreadSearch *>(lParam);
    DWORD pid = 0;
    const DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    if (pid != search->processId || !IsWindowVisible(hwnd))
        return TRUE;

    search->threadId = tid;
    return FALSE;   // 找到可见顶层窗口即终止枚举
}

// WH_GETMESSAGE 钩子必须装到目标的 GUI 线程上：注入侧要在主线程初始化，
// QQuickItem 树访问与事件投递仅在那里有效。按窗口取线程比线程快照可靠。
static DWORD getWindowThreadId(DWORD processId)
{
    WindowThreadSearch search;
    search.processId = processId;
    EnumWindows(&enumWindowProc, reinterpret_cast<LPARAM>(&search));
    return search.threadId;
}

ProcessManager::ProcessManager(QObject *parent) : QObject(parent) {}

ProcessManager::~ProcessManager()
{
    releaseHook();
}

bool ProcessManager::launchTarget(const QString &appPath, const QStringList &args)
{
    if (m_process)
    {
        stopTarget();
    }

    m_targetPath = appPath;
    m_process = new QProcess(this);
    m_process->setProgram(appPath);
    m_process->setArguments(args);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this]() {
        m_targetPid = 0;
        emit targetStopped();
    });

    m_process->start();
    if (!m_process->waitForStarted(5000))
    {
        qWarning() << "[ProcessManager] 启动失败:" << appPath;
        return false;
    }

    m_targetPid = m_process->processId();
    qInfo() << "[ProcessManager] 目标进程已启动, PID:" << m_targetPid;
    emit targetStarted(m_targetPid);
    return true;
}

// ═══════════════════════ DLL 注入 ═══════════════════════

bool ProcessManager::injectDll()
{
    if (!m_targetPid)
    {
        emit injectionResult(false, QStringLiteral("目标进程未运行"));
        return false;
    }

    // 1. 解析 DLL 路径
    QString dllPath = resolveDllPath();
    if (dllPath.isEmpty())
    {
        emit injectionResult(false, QStringLiteral("找不到 QU_Inject.dll"));
        return false;
    }

    // 2. 获取目标 GUI 线程 ID：优先按可见顶层窗口定位，拿不到再回退到线程快照
    const DWORD targetPid = static_cast<DWORD>(m_targetPid);
    DWORD targetThreadId = getWindowThreadId(targetPid);
    if (targetThreadId == 0)
    {
        qInfo() << "[ProcessManager] 未找到目标窗口线程, 回退到线程快照";
        targetThreadId = getMainThreadId(targetPid);
    }
    if (targetThreadId == 0)
    {
        emit injectionResult(false, QStringLiteral("无法获取目标主线程 ID"));
        return false;
    }

    // 3. 本地加载 DLL 以获取模块句柄和导出函数地址
    std::wstring wDllPath = dllPath.toStdWString();
    HMODULE hLocalDll = LoadLibraryW(wDllPath.c_str());
    if (!hLocalDll)
    {
        DWORD err = GetLastError();
        qWarning() << "[ProcessManager] 本地加载 Inject DLL 失败, 错误码:" << err;
        emit injectionResult(false, QStringLiteral("本地加载 Inject DLL 失败"));
        return false;
    }

    HOOKPROC hookProc = reinterpret_cast<HOOKPROC>(GetProcAddress(hLocalDll, "GetMsgProc"));
    if (!hookProc)
    {
        qWarning() << "[ProcessManager] Inject DLL 缺少 GetMsgProc 导出";
        FreeLibrary(hLocalDll);
        emit injectionResult(false, QStringLiteral("Inject DLL 不兼容"));
        return false;
    }

    // 4. 向目标 GUI 线程安装 WH_GETMESSAGE 钩子
    HHOOK hHook = SetWindowsHookEx(WH_GETMESSAGE, hookProc, hLocalDll, targetThreadId);
    if (!hHook)
    {
        DWORD err = GetLastError();
        qWarning() << "[ProcessManager] SetWindowsHookEx 失败, 错误码:" << err;
        FreeLibrary(hLocalDll);
        emit injectionResult(false, QStringLiteral("SetWindowsHookEx 失败"));
        return false;
    }

    // 5. 投递一条空消息，强制目标线程加载 DLL 并调用 HookProc
    if (!PostThreadMessage(targetThreadId, WM_NULL, 0, 0))
    {
        DWORD err = GetLastError();
        qWarning() << "[ProcessManager] PostThreadMessage 失败, 错误码:" << err;
        UnhookWindowsHookEx(hHook);
        FreeLibrary(hLocalDll);
        emit injectionResult(false, QStringLiteral("PostThreadMessage 失败"));
        return false;
    }

    qInfo() << "[ProcessManager] Hook 注入已触发, 目标 PID:" << targetPid
            << "目标线程:" << targetThreadId;

    // 保存句柄：钩子需要在停止目标或销毁时显式卸载，本地 DLL 引用计数也需释放。
    // 既往实现两者都没保存，导致钩子无法卸载、DLL 永不卸载。
    releaseHook();
    m_hookHandle = std::unique_ptr<HookHandle, HookHandleDeleter>(new HookHandle(hHook, hLocalDll));

    emit injectionResult(true, QStringLiteral("Hook 注入已触发"));
    return true;
}

void ProcessManager::releaseHook()
{
    m_hookHandle.reset();
}

// ═══════════════════════ 辅助 ═══════════════════════

QString ProcessManager::resolveDllPath() const
{
    // 优先使用自定义路径
    if (!m_injectDllPath.isEmpty())
    {
        if (QFileInfo::exists(m_injectDllPath))
        {
            return QFileInfo(m_injectDllPath).absoluteFilePath();
        }
        qWarning() << "[ProcessManager] 自定义 DLL 路径不存在:" << m_injectDllPath;
    }

    // 默认查找同目录
    QString exeDir = QCoreApplication::applicationDirPath();
    QString dllPath = exeDir + QStringLiteral("/QU_Inject.dll");
    if (QFileInfo::exists(dllPath))
    {
        return QFileInfo(dllPath).absoluteFilePath();
    }

    // 回退：查找 inject 构建目录（开发阶段）
    dllPath = exeDir + QStringLiteral("/../inject/Release/QU_Inject.dll");
    if (QFileInfo::exists(dllPath))
    {
        QString absPath = QFileInfo(dllPath).absoluteFilePath();
        qInfo() << "[ProcessManager] 使用开发目录 DLL:" << absPath;
        return absPath;
    }

    return QString();
}

void ProcessManager::stopTarget()
{
    releaseHook();
    if (m_process)
    {
        m_process->kill();
        m_process->waitForFinished(3000);
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_targetPid = 0;
}

bool ProcessManager::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}
