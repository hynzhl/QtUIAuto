#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class ProcessManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(quint64 targetPid READ targetPid NOTIFY targetStarted)
    Q_PROPERTY(QString injectDllPath READ injectDllPath WRITE setInjectDllPath)
    Q_PROPERTY(bool running READ isRunning NOTIFY targetStopped)
public:
    explicit ProcessManager(QObject *parent = nullptr);

    Q_INVOKABLE bool launchTarget(const QString &appPath, const QStringList &args = {});
    Q_INVOKABLE bool injectDll();
    Q_INVOKABLE void stopTarget();

    quint64 targetPid() const { return m_targetPid; }
    bool isRunning() const;
    QString injectDllPath() const { return m_injectDllPath; }
    void setInjectDllPath(const QString &path) { m_injectDllPath = path; }

signals:
    void targetStarted(quint64 pid);
    void targetStopped();
    void injectionResult(bool success, const QString &message);

private:
    QString resolveDllPath() const;

    QProcess *m_process = nullptr;
    quint64 m_targetPid = 0;
    QString m_targetPath;
    QString m_injectDllPath;
};

#endif // PROCESSMANAGER_H
