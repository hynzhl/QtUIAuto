#ifndef QTUIAUTOBRIDGEINPUTCONTEXT_H
#define QTUIAUTOBRIDGEINPUTCONTEXT_H

#include <qpa/qplatforminputcontext.h>
#include <QJsonObject>
#include <QPointer>

QT_FORWARD_DECLARE_CLASS(QLocalSocket)

class ControlScanner;
class InputSimulator;
class CommandHandler;

/// ============================================================
/// QtUIAutoBridgeInputContext
///
/// A minimal QPlatformInputContext implementation whose sole purpose is
/// to be auto-loaded by a target Qt application (via QT_IM_MODULE) and
/// start the QtUIAuto agent: connect back to the controller through a
/// named pipe and handle automation commands.
///
/// This avoids any cross-process memory injection (VirtualAllocEx,
/// WriteProcessMemory, CreateRemoteThread) and therefore bypasses EDR/HIPS
/// blocks while remaining non-invasive to the target application code.
/// ============================================================
class QtUIAutoBridgeInputContext : public QPlatformInputContext
{
    Q_OBJECT
public:
    explicit QtUIAutoBridgeInputContext();
    ~QtUIAutoBridgeInputContext() override;

    bool isValid() const override { return true; }

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void initAgent();
    void sendInjectReady();

    QLocalSocket *m_pipe = nullptr;
    ControlScanner *m_scanner = nullptr;
    InputSimulator *m_simulator = nullptr;
    CommandHandler *m_handler = nullptr;
};

#endif // QTUIAUTOBRIDGEINPUTCONTEXT_H
