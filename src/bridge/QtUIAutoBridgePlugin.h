#ifndef QTUIAUTOBRIDGEPLUGIN_H
#define QTUIAUTOBRIDGEPLUGIN_H

#include <qpa/qplatforminputcontextplugin_p.h>

/// ============================================================
/// QtUIAutoBridgePlugin
///
/// Factory plugin for QtUIAutoBridgeInputContext.
/// Loaded automatically by Qt when QT_IM_MODULE=QtUIAutoBridge.
/// ============================================================
class QtUIAutoBridgePlugin : public QPlatformInputContextPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPlatformInputContextFactoryInterface.5.1" FILE "qtuiauto.json")
public:
    explicit QtUIAutoBridgePlugin(QObject *parent = nullptr);

    QPlatformInputContext *create(const QString &key, const QStringList &paramList) override;
};

#endif // QTUIAUTOBRIDGEPLUGIN_H
