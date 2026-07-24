#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

class AccessibilityController;

class ScriptEngine : public QObject
{
    Q_OBJECT
public:
    enum State { Idle, Recording, Playing, Paused };
    Q_ENUM(State)

    explicit ScriptEngine(QObject *parent = nullptr);

    State state() const { return m_state; }

    // Recording
    void startRecording();
    void stopRecording();
    void recordEvent(const QJsonObject &event);

    // Playback
    bool loadScript(const QString &filePath);
    bool loadScriptFromJson(const QJsonArray &steps);
    void startPlayback();
    void stopPlayback();
    void pausePlayback();

    // Script I/O
    bool saveScript(const QString &filePath) const;
    QJsonArray script() const { return m_steps; }

signals:
    void stateChanged(State state);
    void playbackStep(int step, int total);
    void playbackFinished(bool success);
    void recordingSaved(const QString &path);

private:
    State m_state = Idle;
    QJsonArray m_steps;
};

#endif // SCRIPTENGINE_H
