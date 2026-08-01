#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <functional>

class ControlScanner;
class InputSimulator;
class QQuickItem;

/// ============================================================
/// CommandHandler — Named Pipe JSON 命令路由处理器
///
/// 职责边界：
///   - 解析 JSON 命令，提取 action 和参数
///   - 根据 action 分派到 ControlScanner 或 InputSimulator
///   - 组装 JSON 响应返回给 GUI 进程
///   - 不负责网络通信，仅做命令/响应转换
/// ============================================================
class CommandHandler : public QObject
{
    Q_OBJECT

public:
    /// 构造命令路由器
    /// @param scanner 控件扫描器，负责定位目标控件；其生命周期由调用方保证
    /// @param simulator 交互模拟器，负责执行动作；其生命周期由调用方保证
    /// @param parent Qt 父对象
    explicit CommandHandler(ControlScanner *scanner,
                            InputSimulator *simulator,
                            QObject *parent = nullptr);

    /// 处理单条 JSON 命令
    /// @param cmd 命令内容，须含 action 字段
    /// @return 响应 JSON；action 未知或控件定位失败时为错误响应
    QJsonObject handleCommand(const QJsonObject &cmd);

private:
    // ═══════════════ 执行模板 ═══════════════

    /// 定位目标控件后执行 bool 返回型操作（click/setFocus/setValue 等）
    /// @param cmd 原始命令，从中解析目标控件
    /// @param actionName 回填到响应里的动作名
    /// @param action 拿到控件后真正执行的操作
    /// @return 成功或失败的响应 JSON
    QJsonObject execTargetAction(const QJsonObject &cmd,
                                 const QString &actionName,
                                 std::function<bool(QQuickItem *)> action);

    /// 定位目标控件后执行 JSON 返回型查询（getText/getControlInfo 等）
    /// @param cmd 原始命令，从中解析目标控件
    /// @param actionName 回填到响应里的动作名
    /// @param query 拿到控件后执行的查询，返回值作为响应 data
    /// @return 含查询结果的响应 JSON
    QJsonObject execTargetQuery(const QJsonObject &cmd,
                                const QString &actionName,
                                std::function<QJsonObject(QQuickItem *)> query);

    /// 直接执行无需目标控件的操作（listControls/dumpTree/ping）
    /// @param actionName 回填到响应里的动作名
    /// @param action 待执行的操作，返回值作为响应 data
    /// @return 含执行结果的响应 JSON
    QJsonObject execDirect(const QString &actionName,
                           std::function<QJsonObject()> action);

    // ═══════════════ 辅助 ═══════════════

    /// 按命令里的定位字段解析出目标控件
    /// @param cmd 命令内容，支持按 objectName / 类型 / 层级路径定位
    /// @return 命中的控件；未命中时为 nullptr
    QQuickItem *resolveTarget(const QJsonObject &cmd);

    /// 组装统一格式的成功响应
    /// @param status 状态字段值
    /// @param action 动作名
    /// @param data 业务数据，可为空
    /// @return 响应 JSON
    QJsonObject makeResponse(const QString &status,
                             const QString &action,
                             const QJsonObject &data = QJsonObject());

    /// 组装统一格式的错误响应
    /// @param action 动作名
    /// @param message 错误描述
    /// @return status 为 error 的响应 JSON
    QJsonObject makeError(const QString &action, const QString &message);

    // 底层扫描器与模拟器均为 QObject 派生，按规范 §7 用 QPointer 持有。
    QPointer<ControlScanner>   m_scanner;
    QPointer<InputSimulator>   m_simulator;
};

#endif // COMMANDHANDLER_H
