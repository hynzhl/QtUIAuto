#include "ControlScanner.h"
#include <QGuiApplication>
#include <QWindow>
#include <QDebug>

ControlScanner::ControlScanner(QObject *parent)
    : QObject(parent)
{
}

// ═══════════════════════ 窗口枚举 ═══════════════════════

QList<QQuickWindow *> ControlScanner::allWindows() const
{
    QList<QQuickWindow *> result;
    const auto windows = QGuiApplication::allWindows();
    for (auto *w : windows)
    {
        auto *qw = qobject_cast<QQuickWindow *>(w);
        if (qw && qw->isVisible())
        {
            result.append(qw);
        }
    }
    return result;
}

QQuickWindow *ControlScanner::findWindow(const QString &objectName) const
{
    const auto windows = allWindows();
    if (objectName.isEmpty())
    {
        return windows.isEmpty() ? nullptr : windows.first();
    }

    for (auto *win : windows)
    {
        if (win->objectName() == objectName)
        {
            return win;
        }
    }

    qWarning() << "[ControlScanner] 未找到窗口:" << objectName;
    return nullptr;
}

// ═══════════════════════ 控件查找 ═══════════════════════

QQuickItem *ControlScanner::findByObjectName(const QString &name) const
{
    if (name.isEmpty())
    {
        qWarning() << "[ControlScanner] findByObjectName: name 为空";
        return nullptr;
    }

    const auto windows = allWindows();
    for (auto *win : windows)
    {
        auto *root = win->contentItem();
        if (!root) continue;

        if (root->objectName() == name) return root;

        QList<QQuickItem *> results;
        searchByObjectName(root, name, results);
        if (!results.isEmpty()) return results.first();
    }

    qWarning() << "[ControlScanner] 未找到控件 (objectName):" << name;
    return nullptr;
}

QList<QQuickItem *> ControlScanner::findByType(const QString &typeName) const
{
    QList<QQuickItem *> results;
    if (typeName.isEmpty()) return results;

    const auto windows = allWindows();
    for (auto *win : windows)
    {
        QQuickItem *root = win->contentItem();
        if (root) searchByType(root, typeName, results);
    }

    qDebug() << "[ControlScanner] 按类型搜索:" << typeName
             << ", 找到:" << results.size();
    return results;
}

QQuickItem *ControlScanner::findByPath(const QStringList &path) const
{
    if (path.isEmpty())
    {
        qWarning() << "[ControlScanner] findByPath: path 为空";
        return nullptr;
    }

    QQuickWindow *win = findWindow(path.first());
    if (!win)
    {
        qWarning() << "[ControlScanner] findByPath: 未找到窗口:" << path.first();
        return nullptr;
    }

    QQuickItem *current = win->contentItem();
    if (!current) return nullptr;

    for (int i = 1; i < path.size(); ++i)
    {
        const QString &part = path.at(i);
        bool found = false;

        const auto children = current->childItems();
        for (auto *child : children)
        {
            if (child->objectName() == part
                || QString::fromLatin1(child->metaObject()->className()).contains(part))
            {
                current = child;
                found = true;
                break;
            }
        }

        if (!found)
        {
            qWarning() << "[ControlScanner] 路径匹配失败于层级"
                       << i << ":" << part;
            return nullptr;
        }
    }

    return current;
}

// ═══════════════════════ 控件信息 ═══════════════════════

QJsonObject ControlScanner::getControlInfo(QQuickItem *item) const
{
    QJsonObject info;
    if (!item)
    {
        qWarning() << "[ControlScanner] getControlInfo: item 为空";
        return info;
    }

    info["objectName"]  = item->objectName();
    info["typeName"]    = QString::fromLatin1(item->metaObject()->className());
    info["visible"]     = item->isVisible();
    info["enabled"]     = item->isEnabled();
    info["width"]       = item->width();
    info["height"]      = item->height();
    info["x"]           = item->x();
    info["y"]           = item->y();

    auto tryProperty = [&](const char *prop)
    {
        QVariant v = item->property(prop);
        if (v.isValid())
        {
            info[QString::fromLatin1(prop)] = QJsonValue::fromVariant(v);
        }
    };

    tryProperty("text");
    tryProperty("value");
    tryProperty("currentIndex");
    tryProperty("state");

    info["childCount"] = item->childItems().size();
    return info;
}

QJsonObject ControlScanner::dumpTree(QQuickItem *root) const
{
    if (!root)
    {
        QJsonArray windowsArr;
        const auto wins = allWindows();
        for (auto *win : wins)
        {
            QJsonObject winInfo;
            winInfo["windowTitle"] = win->title();
            winInfo["objectName"]  = win->objectName();
            winInfo["width"]       = win->width();
            winInfo["height"]      = win->height();

            QQuickItem *content = win->contentItem();
            if (content)
            {
                QJsonArray children;
                collectChildren(content, children);
                winInfo["children"] = children;
            }
            windowsArr.append(winInfo);
        }

        QJsonObject result;
        result["windows"] = windowsArr;
        return result;
    }

    QJsonArray children;
    collectChildren(root, children);
    QJsonObject info = getControlInfo(root);
    info["children"] = children;
    return info;
}

QRectF ControlScanner::mapToGlobalRect(QQuickItem *item) const
{
    if (!item)
    {
        qWarning() << "[ControlScanner] mapToGlobalRect: item 为空";
        return QRectF();
    }

    QPointF topLeft     = item->mapToGlobal(QPointF(0, 0));
    QPointF bottomRight = item->mapToGlobal(QPointF(item->width(), item->height()));
    return QRectF(topLeft, bottomRight);
}

// ═══════════════════════ 递归遍历 ═══════════════════════

void ControlScanner::collectChildren(QQuickItem *parent, QJsonArray &children) const
{
    if (!parent) return;

    const auto childItems = parent->childItems();
    for (auto *child : childItems)
    {
        if (!child) continue;

        QJsonObject info = getControlInfo(child);

        if (!child->childItems().isEmpty())
        {
            QJsonArray subChildren;
            collectChildren(child, subChildren);
            info["children"] = subChildren;
        }
        children.append(info);
    }
}

void ControlScanner::searchByObjectName(QQuickItem *parent,
                                         const QString &name,
                                         QList<QQuickItem *> &results) const
{
    if (!parent) return;

    const auto childItems = parent->childItems();
    for (auto *child : childItems)
    {
        if (!child) continue;

        if (child->objectName() == name) results.append(child);
        searchByObjectName(child, name, results);
    }
}

void ControlScanner::searchByType(QQuickItem *parent,
                                   const QString &typeName,
                                   QList<QQuickItem *> &results) const
{
    if (!parent) return;

    const auto childItems = parent->childItems();
    for (auto *child : childItems)
    {
        if (!child) continue;

        const QString className = QString::fromLatin1(child->metaObject()->className());
        if (className.contains(typeName, Qt::CaseInsensitive))
        {
            results.append(child);
        }
        searchByType(child, typeName, results);
    }
}
