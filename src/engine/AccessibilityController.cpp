#include "AccessibilityController.h"
#include <QDebug>
#include <QWindow>
#include <QGuiApplication>

AccessibilityController::AccessibilityController(QObject *parent)
    : QObject(parent)
{
}

QAccessibleInterface *AccessibilityController::resolvePath(const QString &path)
{
    if (path.isEmpty() || path == "root" || path == "/") {
        auto *root = QAccessible::queryAccessibleInterface(
            QAccessible::queryAccessibleInterface(qApp->focusWindow()));
        return root;
    }
    // Path format: "window/buttonName" or "mainWindow/child/grandchild"
    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    QAccessibleInterface *current = nullptr;

    // Start from root (focus window)
    auto *root = QAccessible::queryAccessibleInterface(qApp->focusWindow());
    if (!root) return nullptr;
    current = root;

    for (const auto &part : parts) {
        bool found = false;
        for (int i = 0; i < current->childCount(); ++i) {
            auto *child = current->child(i);
            if (child && child->text(QAccessible::Name) == part) {
                if (current != root) delete current;
                current = child;
                found = true;
                break;
            }
            delete child;
        }
        if (!found) {
            if (current != root) delete current;
            return nullptr;
        }
    }
    return current;
}

QJsonArray AccessibilityController::listControls(const QString &rootPath)
{
    QJsonArray result;
    auto *iface = resolvePath(rootPath);
    if (!iface) return result;

    // TODO: recursive tree traversal
    // For now, return direct children
    for (int i = 0; i < iface->childCount(); ++i) {
        auto *child = iface->child(i);
        if (child) {
            QJsonObject info;
            info["role"] = QAccessible::roleToString(child->role());
            info["name"] = child->text(QAccessible::Name);
            info["childCount"] = child->childCount();
            result.append(info);
            delete child;
        }
    }
    return result;
}

bool AccessibilityController::click(const QString &path)
{
    auto *iface = resolvePath(path);
    if (!iface) return false;
    bool ok = iface->actionInterface()
              ? iface->actionInterface()->doAction(QAccessibleActionInterface::pressAction())
              : false;
    delete iface;
    return ok;
}

bool AccessibilityController::typeText(const QString &path, const QString &text)
{
    Q_UNUSED(path)
    Q_UNUSED(text)
    // TODO: implement text input via accessibility
    qWarning() << "typeText not yet implemented";
    return false;
}

QJsonObject AccessibilityController::executeCommand(const QJsonObject &cmd)
{
    QJsonObject result;
    QString action = cmd["action"].toString();

    if (action == "listControls") {
        result["controls"] = listControls(cmd["path"].toString());
        result["status"] = "ok";
    } else if (action == "click") {
        result["status"] = click(cmd["path"].toString()) ? "ok" : "error";
    } else {
        result["status"] = "error";
        result["message"] = "Unknown action: " + action;
    }
    return result;
}
