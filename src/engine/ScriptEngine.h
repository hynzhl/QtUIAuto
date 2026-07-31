#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>

class PipeServer;

class ScriptEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantList script READ scriptVariant NOTIFY stateChanged)
public:
    enum State { Idle, Recording, Playing, Paused };
    Q_ENUM(State)

    explicit ScriptEngine(PipeServer *pipeServer, QObject *parent = nullptr);

    State state() const { return m_state; }

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void recordEvent(const QJsonObject &event);

    Q_INVOKABLE bool loadScript(const QString &filePath);
    Q_INVOKABLE bool loadScriptFromJson(const QVariantList &steps);
    Q_INVOKABLE void startPlayback();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void pausePlayback();

    Q_INVOKABLE bool saveScript(const QString &filePath) const;
    QJsonArray script() const { return m_steps; }
    QVariantList scriptVariant() const;

signals:
    void stateChanged(State state);
    void playbackStep(int step, int total);
    void playbackFinished(bool success);
    void recordingSaved(const QString &path);

private:
    void runNextStep();
    void finishPlayback(bool success);

    State m_state = Idle;
    QJsonArray m_steps;
    PipeServer *m_pipeServer;

    // 当前待执行的步骤下标。回放由定时器逐步驱动，不能用局部变量保存进度。
    int m_currentStep = 0;
};

#endif // SCRIPTENGINE_H
