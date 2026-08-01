#include "QUBridgePlugin.h"
#include "QUBridgeInputContext.h"

QUBridgePlugin::QUBridgePlugin(QObject *parent)
    : QPlatformInputContextPlugin(parent)
{
}

QPlatformInputContext *QUBridgePlugin::create(const QString &key, const QStringList &paramList)
{
    Q_UNUSED(paramList)

    if (key.compare(QStringLiteral("QUBridge"), Qt::CaseInsensitive) != 0)
        return nullptr;

    return new QUBridgeInputContext();
}
