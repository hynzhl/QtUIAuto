#ifndef CONTROLTREE_H
#define CONTROLTREE_H

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

class PipeServer;

/// ============================================================
/// ControlTree — 目标进程控件树查询器（业务逻辑层）
///
/// 职责边界：
///   - 把控件树查询翻译成 listcontrols / dumptree 命令经管道下发
///   - 把响应转成 QVariant 结构，供上层与列表视图直接消费
///   - 管道未连接或命令失败时返回空结果而非抛错，界面因此无需处理异常
///   - 不做控件遍历本身：遍历发生在注入侧的 ControlScanner
/// ============================================================
class ControlTree : public QObject
{
    Q_OBJECT

public:
    /// 构造控件树查询器
    /// @param pipeServer 命名管道服务端，查询命令经它下发；其生命周期由调用方保证
    /// @param parent Qt 父对象
    explicit ControlTree(PipeServer *pipeServer, QObject *parent = nullptr);

    // ── 查询接口 ──

    /// 拉取目标进程的根窗口列表
    /// @return 每个元素为含 objectName / typeName 等字段的 map；
    ///         未连接或查询失败时返回空列表
    Q_INVOKABLE QVariantList getRootWindowList();

    /// 拉取指定窗口的完整控件树
    /// @param windowId 目标窗口的 objectName；留空表示由注入侧自行选定
    /// @return 树形 map；未连接或查询失败时返回只含 Root 节点的空树
    Q_INVOKABLE QVariantMap getControlTree(const QString &windowId = QString());

signals:
    // ═══════════ 信号 ═══════════

    /// 控件树内容可能已变化，提示上层重新拉取
    void treeChanged();

private:
    // 管道服务端为 QObject 派生，按规范 §7 用 QPointer 持有，调用点判空。
    QPointer<PipeServer> m_pipeServer;
};

#endif // CONTROLTREE_H
