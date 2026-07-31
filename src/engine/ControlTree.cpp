#include "ControlTree.h"
#include "PipeServer.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

ControlTree::ControlTree(PipeServer *pipeServer, QObject *parent)
    : QObject(parent)
    , m_pipeServer(pipeServer)
{
}

QVariantList ControlTree::getRootWindowList()
{
    if (!m_pipeServer || !m_pipeServer->isConnected())
    {
        qWarning() << "[ControlTree] Inject DLL 未连接";
        return QVariantList();
    }

    QJsonObject cmd;
    cmd["action"] = QStringLiteral("listcontrols");
    QJsonObject resp = m_pipeServer->sendCommand(cmd);

    if (resp.value("status").toString() != "ok")
    {
        qWarning() << "[ControlTree] listcontrols 失败:" << resp.value("message").toString();
        return QVariantList();
    }

    return resp.value("data").toObject().value("controls").toArray().toVariantList();
}

QVariantMap ControlTree::getControlTree(const QString &windowId)
{
    if (!m_pipeServer || !m_pipeServer->isConnected())
    {
        qWarning() << "[ControlTree] Inject DLL 未连接，返回空树";
        QVariantMap empty;
        empty["type"] = QStringLiteral("Root");
        empty["children"] = QVariantList();
        return empty;
    }

    QJsonObject cmd;
    cmd["action"] = QStringLiteral("dumptree");
    if (!windowId.isEmpty())
    {
        cmd["window"] = windowId;
    }

    QJsonObject resp = m_pipeServer->sendCommand(cmd);
    if (resp.value("status").toString() != "ok")
    {
        qWarning() << "[ControlTree] dumptree 失败:" << resp.value("message").toString();
        QVariantMap empty;
        empty["type"] = QStringLiteral("Root");
        empty["children"] = QVariantList();
        return empty;
    }

    return resp.value("data").toObject().value("tree").toObject().toVariantMap();
}

