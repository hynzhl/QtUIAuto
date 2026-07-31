#ifndef CONTROLSCANNER_H
#define CONTROLSCANNER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
#include <QRectF>

/// ============================================================
/// ControlScanner — QQuickItem 控件树遍历扫描器
/// 通过递归遍历 QQuickItem 对象树，实现控件定位与属性读取
///
/// 职责边界：
///   - 只负责控件定位和树遍历，不执行交互操作
///   - 提供多种查找策略（objectName/类型/层级路径）
///   - 所有操作基于 Qt Quick 公开 API，无需私有头文件
/// ============================================================
class ControlScanner : public QObject
{
    Q_OBJECT
public:
    explicit ControlScanner(QObject *parent = nullptr);

    // ── 窗口枚举 ──
    QList<QQuickWindow *> allWindows() const;
    QQuickWindow *findWindow(const QString &objectName = QString()) const;

    // ── 控件查找 ──
    QQuickItem *findByObjectName(const QString &name) const;
    QList<QQuickItem *> findByType(const QString &typeName) const;
    QQuickItem *findByPath(const QStringList &path) const;

    // ── 控件信息 ──
    QJsonObject getControlInfo(QQuickItem *item) const;
    QJsonObject dumpTree(QQuickItem *root) const;

    /// 将 Item 本地坐标映射到全局屏幕坐标
    QRectF mapToGlobalRect(QQuickItem *item) const;

private:
    // ── 递归遍历 ──
    void collectChildren(QQuickItem *parent, QJsonArray &children) const;
    void searchByObjectName(QQuickItem *parent, const QString &name,
                            QList<QQuickItem *> &results) const;
    void searchByType(QQuickItem *parent, const QString &typeName,
                      QList<QQuickItem *> &results) const;
};

#endif // CONTROLSCANNER_H
