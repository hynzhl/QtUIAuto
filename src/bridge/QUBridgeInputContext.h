#ifndef QUBRIDGEINPUTCONTEXT_H
#define QUBRIDGEINPUTCONTEXT_H

#include <qpa/qplatforminputcontext.h>
#include <QJsonObject>
#include <QPointer>

QT_FORWARD_DECLARE_CLASS(QLocalSocket)

class ControlScanner;
class InputSimulator;
class CommandHandler;

/// ============================================================
/// QUBridgeInputContext
///
/// A minimal QPlatformInputContext implementation whose sole purpose is
/// to be auto-loaded by a target Qt application (via QT_IM_MODULE) and
/// start the QU agent: connect back to the controller through a
/// named pipe and handle automation commands.
///
/// This avoids any cross-process memory injection (VirtualAllocEx,
/// WriteProcessMemory, CreateRemoteThread) and therefore bypasses EDR/HIPS
/// blocks while remaining non-invasive to the target application code.
/// ============================================================
class QUBridgeInputContext : public QPlatformInputContext
{
    Q_OBJECT
public:
    explicit QUBridgeInputContext();
    ~QUBridgeInputContext() override;

    /// Always valid: the context exists solely to bootstrap the agent.
    /// @return true unconditionally.
    bool isValid() const override { return true; }

private slots:
    /// Handles inbound NDJSON command frames from the controller pipe.
    void onReadyRead();
    /// Cleans up agent state when the controller closes the pipe.
    void onDisconnected();

private:
    /// Connects back to the controller pipe and wires up the command handler.
    void initAgent();
    /// Notifies the controller that injection succeeded and the agent is live.
    void sendInjectReady();

    // Agent subcomponents are QObject-derived and parented to this context.
    // Use QPointer per coding standard §7 to avoid dangling references.
    QPointer<QLocalSocket>   m_pipe;
    QPointer<ControlScanner> m_scanner;
    QPointer<InputSimulator> m_simulator;
    QPointer<CommandHandler> m_handler;
};

#endif // QUBRIDGEINPUTCONTEXT_H
