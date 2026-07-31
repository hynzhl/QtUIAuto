#ifndef INPUTSIMULATOR_H
#define INPUTSIMULATOR_H

#include <QObject>
#include <QEvent>
#include <QPointF>
#include <QVariant>
#include <QAccessible>
#include <QAccessibleInterface>
#include <QAccessibleActionInterface>

class QQuickItem;
class QQuickWindow;

/// ============================================================
/// InputSimulator — 双轨交互模拟器
/// 按 窗口场景投递 → MouseArea 直接事件 → item sendEvent → Accessibility
/// 的顺序自动降级执行交互操作。
///
/// 职责边界：
///   - Track 1 (窗口场景投递)：主轨道，把事件发给 QQuickWindow 并用 scene 坐标，
///     由 Qt Quick 自行命中测试与分发，行为最接近真实用户操作
///   - Track 2 (MouseArea 定位)：适用于 Item + MouseArea 组合
///   - Track 3 (item sendEvent)：直接向 item 发送事件
///   - Track 4 (Accessibility)：最后回退。注意 QQuickAccessibleAttached 在未声明
///     Accessible.onPressAction 时，doAction 仅 emit 无接收者的信号而静默失效，
///     所以不能作为主轨道
///   - 属性类操作：getText / setValue 以内容属性为准，Accessibility 仅作回退
/// ============================================================
class InputSimulator : public QObject
{
    Q_OBJECT
public:
    explicit InputSimulator(QObject *parent = nullptr);

    // ── 交互操作 ──
    bool click(QQuickItem *target);
    bool doubleClick(QQuickItem *target);
    bool rightClick(QQuickItem *target);
    bool typeText(QQuickItem *target, const QString &text);
    bool setFocus(QQuickItem *target);

    // ── 属性访问 ──
    QVariant getText(QQuickItem *target) const;
    bool setValue(QQuickItem *target, const QString &property,
                  const QVariant &value);

    // ── 配置 ──
    int trackTimeout() const { return m_trackTimeout; }
    void setTrackTimeout(int ms);

    // ── 获取最后使用的降级路径名（调试用）─
    QString lastTrackUsed() const { return m_lastTrack; }

private:
    // ═══════════ Track 1: 窗口场景投递 ═══════════
    bool tryWindowDeliveryClick(QQuickItem *target);
    bool sendMouseEventViaWindow(QQuickItem *target, QEvent::Type type,
                                 Qt::MouseButton button, Qt::MouseButtons buttons);

    // ═══════════ Track 4: Accessibility ═══════════
    bool tryAccessibleClick(QQuickItem *target);
    bool tryAccessibleSetFocus(QQuickItem *target);
    bool tryAccessibleTypeText(QQuickItem *target, const QString &text);

    // ═══════════ Track 2: MouseArea 直接寻址 ═══════════
    bool tryMouseAreaClick(QQuickItem *target);

    // ═══════════ Track 3: sendEvent 通用回退 ═══════════
    bool trySendEventClick(QQuickItem *target);
    bool trySendEventTypeText(QQuickItem *target, const QString &text);
    bool tryPropertySetText(QQuickItem *target, const QString &text);

    // ═══════════ 事件构造 ═══════════
    bool sendMousePressAt(QQuickItem *receiver, const QPointF &localPos,
                          Qt::MouseButton button = Qt::LeftButton);
    bool sendMouseReleaseAt(QQuickItem *receiver, const QPointF &localPos,
                            Qt::MouseButton button = Qt::LeftButton);
    bool sendMouseDoubleClickAt(QQuickItem *receiver, const QPointF &localPos,
                                Qt::MouseButton button = Qt::LeftButton);
    bool sendKeyPress(QQuickItem *receiver, int key,
                      Qt::KeyboardModifiers mods = Qt::NoModifier);
    bool sendKeyRelease(QQuickItem *receiver, int key,
                        Qt::KeyboardModifiers mods = Qt::NoModifier);

    // ═══════════ 辅助 ═══════════
    QPointF itemCenter(QQuickItem *item) const;
    QPointF itemGlobalCenter(QQuickItem *item) const;
    QQuickItem *findChildMouseArea(QQuickItem *parent) const;

    int m_trackTimeout = 100;
    ulong m_eventTimestamp = 0;
    // getText 为 const 方法，但仍需记录实际取值轨道以便诊断
    mutable QString m_lastTrack;
};

#endif // INPUTSIMULATOR_H
