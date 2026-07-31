#include "ScriptEngine.h"
#include "PipeServer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

namespace {
// 步骤间隔。原先用 QThread::msleep(100) 同步等待，会把 GUI 线程冻住；
// 改用定时器后事件循环仍可响应，stopPlayback() 才能真正生效。
constexpr int kStepIntervalMs = 100;
}

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
        finishPlayback(false);
        return;
    }

    if (!m_pipeServer || !m_pipeServer->isConnected())
    {
        qWarning() << "[ScriptEngine] Inject DLL 未连接，无法回放";
        finishPlayback(false);
        return;
    }

    m_currentStep = 0;
    m_state = Playing;
    emit stateChanged(m_state);

    // 投到事件循环执行，使本函数立即返回，不阻塞调用方（包括 QML 侧）
    QTimer::singleShot(0, this, &ScriptEngine::runNextStep);
}

// 逐步驱动：每执行一步就投递下一步，期间事件循环保持可响应
void ScriptEngine::runNextStep()
{
    // 外部可能已调 stopPlayback() / pausePlayback() 改变状态，此时静默中止链条
    if (m_state != Playing)
        return;

    if (!m_pipeServer || !m_pipeServer->isConnected())
    {
        qWarning() << "[ScriptEngine] 回放中 Inject DLL 断开连接";
        finishPlayback(false);
        return;
    }

    const int totalSteps = m_steps.size();
    if (m_currentStep >= totalSteps)
    {
        finishPlayback(true);
        return;
    }

    const int index = m_currentStep++;
    QJsonObject step = m_steps[index].toObject();
    emit playbackStep(index + 1, totalSteps);

    if (!step.contains("action"))
    {
        qWarning() << "[ScriptEngine] 步骤" << index << "缺少 action";
        finishPlayback(false);
        return;
    }

    QJsonObject response = m_pipeServer->sendCommand(step);
    const QString status = response.value("status").toString();
    if (status != "ok")
    {
        qWarning() << "[ScriptEngine] 步骤" << index
                    << "(" << step.value("action").toString() << ") 失败:"
                    << response.value("message").toString();
        finishPlayback(false);
        return;
    }

    QTimer::singleShot(kStepIntervalMs, this, &ScriptEngine::runNextStep);
}

void ScriptEngine::finishPlayback(bool success)
{
    m_state = Idle;
    emit stateChanged(m_state);
    emit playbackFinished(success);
}

void ScriptEngine::stopPlayback()
{
    // 状态置 Idle 后，已投递的 runNextStep 会在首行自行退出
    finishPlayback(false);
}

void ScriptEngine::pausePlayback()
{
    m_state = Paused;
    emit stateChanged(m_state);
}
