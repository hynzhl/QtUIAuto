#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
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
    explicit CommandHandler(ControlScanner *scanner,
                            InputSimulator *simulator,
                            QObject *parent = nullptr);

    /// 处理单条 JSON 命令，返回 JSON 响应
    QJsonObject handleCommand(const QJsonObject &cmd);

private:
    // ═══════════════ 执行模板 ═══════════════

    /// 定位目标控件后执行 bool 返回型操作（click/setFocus/setValue 等）
    QJsonObject execTargetAction(const QJsonObject &cmd,
                                 const QString &actionName,
                                 std::function<bool(QQuickItem *)> action);

    /// 定位目标控件后执行 JSON 返回型查询（getText/getControlInfo 等）
    QJsonObject execTargetQuery(const QJsonObject &cmd,
                                const QString &actionName,
                                std::function<QJsonObject(QQuickItem *)> query);

    /// 直接执行无需目标控件的操作（listControls/dumpTree/ping）
    QJsonObject execDirect(const QString &actionName,
                           std::function<QJsonObject()> action);

    // ═══════════════ 辅助 ═══════════════
    QQuickItem *resolveTarget(const QJsonObject &cmd);
    QJsonObject makeResponse(const QString &status,
                             const QString &action,
                             const QJsonObject &data = QJsonObject());
    QJsonObject makeError(const QString &action, const QString &message);

    ControlScanner   *m_scanner   = nullptr;
    InputSimulator   *m_simulator = nullptr;
};

#endif // COMMANDHANDLER_H
