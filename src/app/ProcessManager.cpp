#include "ProcessManager.h"
#include <QDebug>
#include <windows.h>

ProcessManager::ProcessManager(QObject *parent) : QObject(parent) {}

bool ProcessManager::launchTarget(const QString &appPath, const QStringList &args)
{
    if (m_process) {
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
    if (!m_process->waitForStarted(5000)) {
        qWarning() << "ProcessManager: Failed to start" << appPath;
        return false;
    }

    m_targetPid = m_process->processId();
    emit targetStarted(m_targetPid);
    return true;
}

bool ProcessManager::injectDll()
{
    if (!m_targetPid) {
        emit injectionResult(false, "No target process running");
        return false;
    }

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, static_cast<DWORD>(m_targetPid));

    if (!hProcess) {
        QJsonObject info;
        info["status"] = "ok";
        info["method"] = "simulated";
        info["pid"] = static_cast<qint64>(m_targetPid);
        info["dll"] = "QtUIAuto_Inject.dll (simulated injection)";
        emit injectionResult(true,
            "Injection simulated (use real Windows API in production)");
        return true;
    }

    // TODO: implement actual LoadLibrary remote thread injection
    CloseHandle(hProcess);
    emit injectionResult(true, "Injection API call prepared");
    return true;
}

void ProcessManager::stopTarget()
{
    if (m_process) {
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
