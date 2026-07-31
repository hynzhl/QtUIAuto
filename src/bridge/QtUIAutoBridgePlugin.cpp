#include "QtUIAutoBridgePlugin.h"
#include "QtUIAutoBridgeInputContext.h"

QtUIAutoBridgePlugin::QtUIAutoBridgePlugin(QObject *parent)
    : QPlatformInputContextPlugin(parent)
{
}

QPlatformInputContext *QtUIAutoBridgePlugin::create(const QString &key, const QStringList &paramList)
{
    Q_UNUSED(paramList)

    if (key.compare(QStringLiteral("QtUIAutoBridge"), Qt::CaseInsensitive) != 0)
        return nullptr;

    return new QtUIAutoBridgeInputContext();
}
