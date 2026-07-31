#include "CommandHandler.h"
#include "ControlScanner.h"
#include "InputSimulator.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QDebug>

CommandHandler::CommandHandler(ControlScanner *scanner,
                                InputSimulator *simulator,
                                QObject *parent)
    : QObject(parent)
    , m_scanner(scanner)
    , m_simulator(simulator)
{
}

QJsonObject CommandHandler::handleCommand(const QJsonObject &cmd)
{
    const QString action = cmd.value("action").toString().toLower();
    qDebug() << "[CommandHandler] 收到命令:" << action;

    // ─── 交互命令（bool 返回型）───
    if (action == "click")
        return execTargetAction(cmd, action,
            [this](QQuickItem *t) { return m_simulator->click(t); });

    if (action == "doubleclick")
        return execTargetAction(cmd, action,
            [this](QQuickItem *t) { return m_simulator->doubleClick(t); });

    if (action == "rightclick")
        return execTargetAction(cmd, action,
            [this](QQuickItem *t) { return m_simulator->rightClick(t); });

    if (action == "setfocus")
        return execTargetAction(cmd, action,
            [this](QQuickItem *t) { return m_simulator->setFocus(t); });

    // ─── 交互命令（需额外参数校验）───
    if (action == "typetext")
    {
        const QString text = cmd.value("text").toString();
        if (text.isEmpty()) return makeError(action, "text 参数为空");
        return execTargetAction(cmd, action,
            [this, &text](QQuickItem *t) { return m_simulator->typeText(t, text); });
    }

    if (action == "setvalue")
    {
        const QString property = cmd.value("property").toString();
        if (property.isEmpty()) return makeError(action, "property 参数为空");
        const QVariant value = cmd.value("value").toVariant();
        return execTargetAction(cmd, action,
            [this, &property, &value](QQuickItem *t) { return m_simulator->setValue(t, property, value); });
    }

    // ─── 查询命令（JSON 返回型）───
    if (action == "gettext")
        return execTargetQuery(cmd, action,
            [this](QQuickItem *t) -> QJsonObject {
                // 带回实际取值轨道，便于定位“读到的不是控件内容”这类问题
                const QString text = m_simulator->getText(t).toString();
                return {{"text", text}, {"method", m_simulator->lastTrackUsed()}};
            });

    if (action == "getcontrolinfo")
        return execTargetQuery(cmd, action,
            [this](QQuickItem *t) -> QJsonObject { return {{"info", m_scanner->getControlInfo(t)}}; });

    if (action == "getscreenrect")
        return execTargetQuery(cmd, action,
            [this](QQuickItem *t) -> QJsonObject {
                QRectF r = m_scanner->mapToGlobalRect(t);
                QJsonObject rectObj;
                rectObj["x"]      = r.x();
                rectObj["y"]      = r.y();
                rectObj["width"]  = r.width();
                rectObj["height"] = r.height();
                return {{"rect", rectObj}};
            });

    // ─── 树遍历命令（无目标控件）───
    // controls 必须是窗口数组：dumpTree 返回的是 {"windows": [...]}，
    // 若原样塞给 controls，主程序侧按数组解析会恒得空列表
    if (action == "listcontrols")
        return execDirect(action,
            [this]() -> QJsonObject {
                return {{"controls", m_scanner->dumpTree(nullptr).value("windows").toArray()}};
            });

    if (action == "dumptree")
    {
        const QString windowName = cmd.value("window").toString();
        QQuickItem *root = nullptr;
        if (!windowName.isEmpty())
        {
            QQuickWindow *win = m_scanner->findWindow(windowName);
            if (win) root = win->contentItem();
        }
        return execDirect(action,
            [this, root]() -> QJsonObject { return {{"tree", m_scanner->dumpTree(root)}}; });
    }

    if (action == "ping")
        return execDirect(action,
            []() -> QJsonObject { return {{"version", "0.1.0"}}; });

    return makeError(action, QStringLiteral("未知命令: ") + action);
}

// ═══════════════════════ 执行模板 ═══════════════════════

QJsonObject CommandHandler::execTargetAction(
    const QJsonObject &cmd,
    const QString &actionName,
    std::function<bool(QQuickItem *)> action)
{
    QQuickItem *target = resolveTarget(cmd);
    if (!target) return makeError(actionName, QStringLiteral("未找到目标控件"));

    bool ok = action(target);
    QJsonObject data;
    data["method"] = m_simulator->lastTrackUsed();
    return makeResponse(ok ? "ok" : "error", actionName, data);
}

QJsonObject CommandHandler::execTargetQuery(
    const QJsonObject &cmd,
    const QString &actionName,
    std::function<QJsonObject(QQuickItem *)> query)
{
    QQuickItem *target = resolveTarget(cmd);
    if (!target) return makeError(actionName, QStringLiteral("未找到目标控件"));

    QJsonObject data = query(target);
    return makeResponse("ok", actionName, data);
}

QJsonObject CommandHandler::execDirect(
    const QString &actionName,
    std::function<QJsonObject()> action)
{
    QJsonObject data = action();
    return makeResponse("ok", actionName, data);
}

// ═══════════════════════ 辅助 ═══════════════════════

QQuickItem *CommandHandler::resolveTarget(const QJsonObject &cmd)
{
    const QString findBy = cmd.value("findBy").toString(
        QStringLiteral("objectName"));
    const QString target = cmd.value("target").toString();

    if (target.isEmpty())
    {
        qWarning() << "[CommandHandler] resolveTarget: target 为空";
        return nullptr;
    }

    if (findBy == QStringLiteral("objectName"))
    {
        return m_scanner->findByObjectName(target);
    }

    if (findBy == QStringLiteral("type"))
    {
        QList<QQuickItem *> items = m_scanner->findByType(target);
        return items.isEmpty() ? nullptr : items.first();
    }

    if (findBy == QStringLiteral("path"))
    {
        QStringList path = target.split('/', Qt::SkipEmptyParts);
        return m_scanner->findByPath(path);
    }

    if (findBy == QStringLiteral("window"))
    {
        QQuickWindow *win = m_scanner->findWindow(target);
        return win ? win->contentItem() : nullptr;
    }

    qWarning() << "[CommandHandler] 未知定位方式:" << findBy;
    return nullptr;
}

QJsonObject CommandHandler::makeResponse(const QString &status,
                                          const QString &action,
                                          const QJsonObject &data)
{
    QJsonObject resp;
    resp["status"] = status;
    resp["action"] = action;
    if (!data.isEmpty())
    {
        resp["data"] = data;
    }
    return resp;
}

QJsonObject CommandHandler::makeError(const QString &action,
                                       const QString &message)
{
    QJsonObject resp;
    resp["status"] = QStringLiteral("error");
    resp["action"] = action;
    resp["message"] = message;
    qWarning() << "[CommandHandler] 错误 -" << action << ":" << message;
    return resp;
}
