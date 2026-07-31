#include "ProcessManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <windows.h>
#include <TlHelp32.h>

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

ProcessManager::ProcessManager(QObject *parent) : QObject(parent) {}

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
        emit injectionResult(false, QStringLiteral("找不到 QtUIAuto_Inject.dll"));
        return false;
    }

    // 2. 获取目标主线程 ID
    const DWORD targetPid = static_cast<DWORD>(m_targetPid);
    const DWORD targetThreadId = getMainThreadId(targetPid);
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
    emit injectionResult(true, QStringLiteral("Hook 注入已触发"));

    // 钩子句柄和 DLL 模块在目标进程中保持有效；当前进程可以释放本地引用计数。
    // 进程退出时由系统清理钩子。
    return true;
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
    QString dllPath = exeDir + QStringLiteral("/QtUIAuto_Inject.dll");
    if (QFileInfo::exists(dllPath))
    {
        return QFileInfo(dllPath).absoluteFilePath();
    }

    // 回退：查找 inject 构建目录（开发阶段）
    dllPath = exeDir + QStringLiteral("/../inject/Release/QtUIAuto_Inject.dll");
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
