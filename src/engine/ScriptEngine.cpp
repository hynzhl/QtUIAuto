#include "ScriptEngine.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

ScriptEngine::ScriptEngine(QObject *parent) : QObject(parent) {}

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
    step["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    m_steps.append(step);
}

bool ScriptEngine::loadScript(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ScriptEngine: Cannot open" << filePath;
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        qWarning() << "ScriptEngine: Invalid script format";
        return false;
    }
    m_steps = doc.array();
    return true;
}

bool ScriptEngine::loadScriptFromJson(const QJsonArray &steps)
{
    m_steps = steps;
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

void ScriptEngine::startPlayback()
{
    m_state = Playing;
    emit stateChanged(m_state);
    // TODO: iterate m_steps and execute via AccessibilityController
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
