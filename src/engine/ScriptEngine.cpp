#include "ScriptEngine.h"
#include "PipeServer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QThread>

ScriptEngine::ScriptEngine(PipeServer *pipeServer, QObject *parent)
    : QObject(parent)
    , m_pipeServer(pipeServer)
{
}

void ScriptEngine::startRecording()
{
    m_steps = QJsonArray();
    m_state = Recording;
    emit stateChanged(m_state);
}

void ScriptEngine::stopRecording()
{
    m_state = Idle;
    emit stateChanged(m_state);
}

void ScriptEngine::recordEvent(const QJsonObject &event)
{
    if (m_state != Recording) return;

    QJsonObject step = event;
    m_steps.append(step);
}

bool ScriptEngine::loadScript(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "[ScriptEngine] 无法打开脚本文件:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
    {
        qWarning() << "[ScriptEngine] 无效的脚本格式";
        return false;
    }

    m_steps = doc.array();
    return true;
}

bool ScriptEngine::loadScriptFromJson(const QVariantList &steps)
{
    QJsonArray arr = QJsonArray::fromVariantList(steps);
    m_steps = arr;
    return true;
}

bool ScriptEngine::saveScript(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(m_steps);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QVariantList ScriptEngine::scriptVariant() const
{
    return m_steps.toVariantList();
}

void ScriptEngine::startPlayback()
{
    if (m_steps.isEmpty())
    {
        qWarning() << "[ScriptEngine] 无可回放的步骤";
        m_state = Idle;
        emit stateChanged(m_state);
        emit playbackFinished(false);
        return;
    }

    if (!m_pipeServer || !m_pipeServer->isConnected())
    {
        qWarning() << "[ScriptEngine] Inject DLL 未连接，无法回放";
        m_state = Idle;
        emit stateChanged(m_state);
        emit playbackFinished(false);
        return;
    }

    m_state = Playing;
    emit stateChanged(m_state);

    const int totalSteps = m_steps.size();
    bool allSuccess = true;

    for (int i = 0; i < totalSteps; ++i)
    {
        QJsonObject step = m_steps[i].toObject();
        emit playbackStep(i + 1, totalSteps);

        if (!step.contains("action"))
        {
            qWarning() << "[ScriptEngine] 步骤" << i << "缺少 action";
            allSuccess = false;
            break;
        }

        QJsonObject response = m_pipeServer->sendCommand(step);
        const QString status = response.value("status").toString();
        if (status != "ok")
        {
            qWarning() << "[ScriptEngine] 步骤" << i
                        << "(" << step.value("action").toString() << ") 失败:"
                        << response.value("message").toString();
            allSuccess = false;
            break;
        }

        QThread::msleep(100);
    }

    m_state = Idle;
    emit stateChanged(m_state);
    emit playbackFinished(allSuccess);
}

void ScriptEngine::stopPlayback()
{
    m_state = Idle;
    emit stateChanged(m_state);
    emit playbackFinished(false);
}

void ScriptEngine::pausePlayback()
{
    m_state = Paused;
    emit stateChanged(m_state);
}
