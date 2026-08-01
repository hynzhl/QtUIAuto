#include "AppContext.h"
#include "app/ProcessManager.h"
#include "engine/PipeServer.h"
#include "engine/ControlTree.h"
#include "engine/ScriptEngine.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace {
// 脚本落盘的固定文件名。保存与加载走同一路径，界面上的「保存」与「打开」
// 因此构成闭环，用户无需在 QML 里指定任何路径。
constexpr char kScriptFileName[] = "recorded_script.json";

// 脚本目录相对应用数据目录的子路径
constexpr char kScriptSubDir[] = "scripts";
}

AppContext::AppContext(ProcessManager *process,
                       PipeServer *pipe,
                       ControlTree *tree,
                       ScriptEngine *script,
                       QObject *parent)
    : QObject(parent)
    , m_process(process)
    , m_pipe(pipe)
    , m_tree(tree)
    , m_script(script)
{
    // 底层引擎信号一律在此转发为门面信号（观察者模式，规范 §10）。
    // QML 只订阅门面，不认识任何引擎对象。
    if (m_pipe)
    {
        connect(m_pipe, &PipeServer::clientConnected, this, &AppContext::connectionChanged);
        connect(m_pipe, &PipeServer::clientDisconnected, this, &AppContext::connectionChanged);
        connect(m_pipe, &PipeServer::injectReady, this, &AppContext::connectionChanged);
    }

    if (m_process)
    {
        connect(m_process, &ProcessManager::injectionResult, this, &AppContext::injectionResult);
    }

    if (m_script)
    {
        // recording / playing / script 三个属性同源于脚本引擎状态，
        // 统一由 scriptStateChanged 触发重新求值。
        connect(m_script, &ScriptEngine::stateChanged, this, &AppContext::scriptStateChanged);
        connect(m_script, &ScriptEngine::playbackStep, this, &AppContext::playbackStep);
        connect(m_script, &ScriptEngine::playbackFinished, this, &AppContext::playbackFinished);
    }
}

bool AppContext::isConnected() const
{
    return m_pipe && m_pipe->isConnected();
}

bool AppContext::isRecording() const
{
    return m_script && m_script->isRecording();
}

bool AppContext::isPlaying() const
{
    return m_script && m_script->isPlaying();
}

QVariantList AppContext::script() const
{
    return m_script ? m_script->scriptVariant() : QVariantList();
}

bool AppContext::launchTarget(const QString &appPath)
{
    if (!m_process)
    {
        qWarning() << "[AppContext] 进程管理器不可用, 无法启动目标";
        return false;
    }

    qDebug() << "[AppContext] 启动目标进程:" << appPath;
    return m_process->launchTarget(appPath);
}

bool AppContext::injectDll()
{
    if (!m_process)
    {
        qWarning() << "[AppContext] 进程管理器不可用, 无法注入";
        return false;
    }

    return m_process->injectDll();
}

void AppContext::stopTarget()
{
    if (!m_process)
    {
        return;
    }

    qDebug() << "[AppContext] 停止目标进程";
    m_process->stopTarget();
}

void AppContext::toggleRecording()
{
    if (!m_script)
    {
        qWarning() << "[AppContext] 脚本引擎不可用, 无法切换录制状态";
        return;
    }

    if (m_script->isRecording())
    {
        m_script->stopRecording();
    }
    else
    {
        m_script->startRecording();
    }
}

void AppContext::togglePlayback()
{
    if (!m_script)
    {
        qWarning() << "[AppContext] 脚本引擎不可用, 无法切换回放状态";
        return;
    }

    if (m_script->isPlaying())
    {
        m_script->stopPlayback();
    }
    else
    {
        m_script->startPlayback();
    }
}

bool AppContext::saveScript()
{
    const QString path = scriptFilePath();
    if (!m_script || !ensureScriptDir())
    {
        qWarning() << "[AppContext] 脚本保存失败:" << path;
        emit scriptSaved(false, path);
        return false;
    }

    // 必须以返回值判定成败：早先界面无条件显示「脚本已保存」，
    // 写盘失败时同样报成功，是典型的谎报。
    const bool success = m_script->saveScript(path);
    if (success)
    {
        qInfo() << "[AppContext] 脚本已保存:" << path;
    }
    else
    {
        qWarning() << "[AppContext] 脚本保存失败:" << path;
    }

    emit scriptSaved(success, path);
    return success;
}

bool AppContext::loadScript()
{
    const QString path = scriptFilePath();
    if (!m_script)
    {
        qWarning() << "[AppContext] 脚本引擎不可用, 无法加载:" << path;
        emit scriptLoaded(false, path);
        return false;
    }

    const bool success = m_script->loadScript(path);
    if (success)
    {
        qInfo() << "[AppContext] 脚本已加载:" << path;
        emit scriptStateChanged();
    }
    else
    {
        qWarning() << "[AppContext] 脚本加载失败:" << path;
    }

    emit scriptLoaded(success, path);
    return success;
}

QString AppContext::scriptFilePath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QString("%1/%2").arg(kScriptSubDir, kScriptFileName));
}

QVariantList AppContext::rootWindowList()
{
    if (!m_tree)
    {
        qWarning() << "[AppContext] 控件树不可用";
        return QVariantList();
    }

    return m_tree->getRootWindowList();
}

bool AppContext::ensureScriptDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString dir  = QDir(base).filePath(QString::fromLatin1(kScriptSubDir));
    if (QDir().mkpath(dir))
    {
        return true;
    }

    qWarning() << "[AppContext] 脚本目录创建失败:" << dir;
    return false;
}
