#ifndef QUBRIDGEPLUGIN_H
#define QUBRIDGEPLUGIN_H

#include <qpa/qplatforminputcontextplugin_p.h>

/// ============================================================
/// QUBridgePlugin
///
/// Factory plugin for QUBridgeInputContext.
/// Loaded automatically by Qt when QT_IM_MODULE=QUBridge.
/// ============================================================
class QUBridgePlugin : public QPlatformInputContextPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPlatformInputContextFactoryInterface.5.1" FILE "qu.json")
public:
    explicit QUBridgePlugin(QObject *parent = nullptr);

    /// Creates the input context instance when the key matches this plugin.
    /// @param key       The requested input-method key (from QT_IM_MODULE).
    /// @param paramList Extra parameters passed by the platform (unused).
    /// @return A new QUBridgeInputContext, or nullptr if the key mismatches.
    QPlatformInputContext *create(const QString &key, const QStringList &paramList) override;
};

#endif // QUBRIDGEPLUGIN_H
