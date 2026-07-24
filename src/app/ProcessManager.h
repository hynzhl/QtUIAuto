#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>

class ProcessManager : public QObject
{
    Q_OBJECT
public:
    explicit ProcessManager(QObject *parent = nullptr);

    bool launchTarget(const QString &appPath, const QStringList &args = {});
    bool injectDll();
    void stopTarget();

    quint64 targetPid() const { return m_targetPid; }
    bool isRunning() const;

signals:
    void targetStarted(quint64 pid);
    void targetStopped();
    void injectionResult(bool success, const QString &message);

private:
    QProcess *m_process = nullptr;
    quint64 m_targetPid = 0;
    QString m_targetPath;
};

#endif // PROCESSMANAGER_H
