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
    /// 构造控件扫描器
    /// @param parent Qt 父对象
    explicit ControlScanner(QObject *parent = nullptr);

    // ── 窗口枚举 ──

    /// @return 当前进程内所有 QQuickWindow
    QList<QQuickWindow *> allWindows() const;

    /// 按名查找窗口
    /// @param objectName 目标窗口的 objectName；留空则取第一个窗口
    /// @return 命中的窗口；未命中时为 nullptr
    QQuickWindow *findWindow(const QString &objectName = QString()) const;

    // ── 控件查找 ──

    /// 按 objectName 查找单个控件
    /// @param name 目标控件的 objectName
    /// @return 首个命中的控件；未命中时为 nullptr
    QQuickItem *findByObjectName(const QString &name) const;

    /// 按类型名查找所有控件
    /// @param typeName QML 类型名（如 Button）
    /// @return 所有命中的控件
    QList<QQuickItem *> findByType(const QString &typeName) const;

    /// 按层级路径逐层下钻查找
    /// @param path 从窗口开始的 objectName 序列
    /// @return 路径末端的控件；任一层级未命中时为 nullptr
    QQuickItem *findByPath(const QStringList &path) const;

    // ── 控件信息 ──

    /// 读取单个控件的属性快照
    /// @param item 目标控件
    /// @return 含 objectName / typeName / 几何位置等字段的 JSON
    QJsonObject getControlInfo(QQuickItem *item) const;

    /// 递归导出以 root 为根的控件树
    /// @param root 子树根控件
    /// @return 树形 JSON，子节点放在 children 数组里
    QJsonObject dumpTree(QQuickItem *root) const;

    /// 将 Item 本地坐标映射到全局屏幕坐标
    /// @param item 目标控件
    /// @return 全局屏幕坐标系下的矩形
    QRectF mapToGlobalRect(QQuickItem *item) const;

private:
    // ── 递归遍历 ──

    /// 收集 parent 的全部后代信息
    /// @param parent 子树根
    /// @param children 输出参数，逐个追加子节点 JSON
    void collectChildren(QQuickItem *parent, QJsonArray &children) const;

    /// 按 objectName 递归搜索
    /// @param parent 子树根
    /// @param name 目标 objectName
    /// @param results 输出参数，追加所有命中项
    void searchByObjectName(QQuickItem *parent, const QString &name,
                            QList<QQuickItem *> &results) const;

    /// 按类型名递归搜索
    /// @param parent 子树根
    /// @param typeName 目标类型名
    /// @param results 输出参数，追加所有命中项
    void searchByType(QQuickItem *parent, const QString &typeName,
                      QList<QQuickItem *> &results) const;
};

#endif // CONTROLSCANNER_H
