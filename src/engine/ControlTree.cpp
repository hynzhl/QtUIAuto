#include "ControlTree.h"
#include <QGuiApplication>
#include <QWindow>
#include <QAccessible>
#include <QAccessibleInterface>

ControlTree::ControlTree(QObject *parent) : QObject(parent) {}

QJsonArray ControlTree::getRootWindowList()
{
    QJsonArray windows;
    for (auto *w : QGuiApplication::topLevelWindows()) {
        QJsonObject win;
        win["title"] = w->title();
        win["objectName"] = w->objectName();
        win["visible"] = w->isVisible();
        windows.append(win);
    }
    return windows;
}

QJsonObject ControlTree::getControlTree(const QString &windowId)
{
    Q_UNUSED(windowId)
    QJsonObject root;
    root["type"] = "Root";
    root["children"] = QJsonArray();
    return root;
}
