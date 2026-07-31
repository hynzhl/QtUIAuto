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
    ~ProcessManager() override;

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
    void releaseHook();

    QProcess *m_process = nullptr;
    quint64 m_targetPid = 0;
    QString m_targetPath;
    QString m_injectDllPath;

    // 钩子句柄与本地 DLL 模块句柄。必须保存才能卸载钩子并释放引用计数；
    // 类型用 void * 而非 HHOOK / HMODULE，是为了不把 windows.h 的宏污染到每个包含者。
    void *m_hook = nullptr;
    void *m_hookDll = nullptr;
};

#endif // PROCESSMANAGER_H
