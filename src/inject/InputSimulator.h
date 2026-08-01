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
    /// 构造交互模拟器
    /// @param parent Qt 父对象
    explicit InputSimulator(QObject *parent = nullptr);

    // ── 交互操作 ──

    /// 单击控件，按四条轨道自动降级
    /// @param target 目标控件
    /// @return 任一轨道成功即为 true；全部失败时为 false
    bool click(QQuickItem *target);

    /// 双击控件
    /// @param target 目标控件
    /// @return 是否成功
    bool doubleClick(QQuickItem *target);

    /// 右击控件
    /// @param target 目标控件
    /// @return 是否成功
    bool rightClick(QQuickItem *target);

    /// 向控件输入文本
    /// @param target 目标控件
    /// @param text 待输入的文本
    /// @return 是否成功
    bool typeText(QQuickItem *target, const QString &text);

    /// 把焦点给到控件
    /// @param target 目标控件
    /// @return 是否成功
    bool setFocus(QQuickItem *target);

    // ── 属性访问 ──

    /// 读取控件的文本内容。以内容属性为准，Accessibility 仅作回退。
    /// @param target 目标控件
    /// @return 文本值；取不到时为无效 QVariant
    QVariant getText(QQuickItem *target) const;

    /// 设置控件的任意属性
    /// @param target 目标控件
    /// @param property 属性名
    /// @param value 待写入的值
    /// @return 是否写入成功
    bool setValue(QQuickItem *target, const QString &property,
                  const QVariant &value);

    // ── 配置 ──

    /// @return 单条轨道的等待超时（毫秒）
    int trackTimeout() const { return m_trackTimeout; }

    /// 调整单条轨道的等待超时
    /// @param ms 超时毫秒数
    void setTrackTimeout(int ms);

    /// @return 最后一次实际生效的降级轨道名，仅用于诊断
    QString lastTrackUsed() const { return m_lastTrack; }

private:
    // ═══════════ Track 1: 窗口场景投递 ═══════════

    /// 把事件发给窗口并用 scene 坐标，由 Qt Quick 自行命中测试
    /// @param target 目标控件
    /// @return 本轨道是否成功
    bool tryWindowDeliveryClick(QQuickItem *target);

    /// 经窗口发送一条鼠标事件
    /// @param target 目标控件，用于推算 scene 坐标
    /// @param type 事件类型
    /// @param button 本次变化的按钮
    /// @param buttons 事件发生时处于按下状态的按钮集合
    /// @return 事件是否被接受
    bool sendMouseEventViaWindow(QQuickItem *target, QEvent::Type type,
                                 Qt::MouseButton button, Qt::MouseButtons buttons);

    // ═══════════ Track 4: Accessibility ═══════════

    /// 经无障碍接口触发点击
    /// @param target 目标控件
    /// @return 本轨道是否成功
    bool tryAccessibleClick(QQuickItem *target);

    /// 经无障碍接口设焦点
    /// @param target 目标控件
    /// @return 本轨道是否成功
    bool tryAccessibleSetFocus(QQuickItem *target);

    /// 经无障碍接口输入文本
    /// @param target 目标控件
    /// @param text 待输入的文本
    /// @return 本轨道是否成功
    bool tryAccessibleTypeText(QQuickItem *target, const QString &text);

    // ═══════════ Track 2: MouseArea 直接寻址 ═══════════

    /// 定位子树里的 MouseArea 并直接向其发事件
    /// @param target 目标控件
    /// @return 本轨道是否成功
    bool tryMouseAreaClick(QQuickItem *target);

    // ═══════════ Track 3: sendEvent 通用回退 ═══════════

    /// 直接向 item 发送鼠标事件
    /// @param target 目标控件
    /// @return 本轨道是否成功
    bool trySendEventClick(QQuickItem *target);

    /// 直接向 item 发送按键事件
    /// @param target 目标控件
    /// @param text 待输入的文本
    /// @return 本轨道是否成功
    bool trySendEventTypeText(QQuickItem *target, const QString &text);

    /// 直接写控件的文本属性，作为输入的最后回退
    /// @param target 目标控件
    /// @param text 待写入的文本
    /// @return 是否写入成功
    bool tryPropertySetText(QQuickItem *target, const QString &text);

    // ═══════════ 事件构造 ═══════════

    /// 发送鼠标按下
    /// @param receiver 接收者
    /// @param localPos 接收者本地坐标
    /// @param button 按下的按钮
    /// @return 事件是否被接受
    bool sendMousePressAt(QQuickItem *receiver, const QPointF &localPos,
                          Qt::MouseButton button = Qt::LeftButton);

    /// 发送鼠标释放
    /// @param receiver 接收者
    /// @param localPos 接收者本地坐标
    /// @param button 释放的按钮
    /// @return 事件是否被接受
    bool sendMouseReleaseAt(QQuickItem *receiver, const QPointF &localPos,
                            Qt::MouseButton button = Qt::LeftButton);

    /// 发送鼠标双击
    /// @param receiver 接收者
    /// @param localPos 接收者本地坐标
    /// @param button 双击的按钮
    /// @return 事件是否被接受
    bool sendMouseDoubleClickAt(QQuickItem *receiver, const QPointF &localPos,
                                Qt::MouseButton button = Qt::LeftButton);

    /// 发送按键按下
    /// @param receiver 接收者
    /// @param key Qt::Key 键值
    /// @param mods 修饰键
    /// @return 事件是否被接受
    bool sendKeyPress(QQuickItem *receiver, int key,
                      Qt::KeyboardModifiers mods = Qt::NoModifier);

    /// 发送按键释放
    /// @param receiver 接收者
    /// @param key Qt::Key 键值
    /// @param mods 修饰键
    /// @return 事件是否被接受
    bool sendKeyRelease(QQuickItem *receiver, int key,
                        Qt::KeyboardModifiers mods = Qt::NoModifier);

    // ═══════════ 辅助 ═══════════

    /// @param item 目标控件
    /// @return 控件中心的本地坐标
    QPointF itemCenter(QQuickItem *item) const;

    /// @param item 目标控件
    /// @return 控件中心的全局屏幕坐标
    QPointF itemGlobalCenter(QQuickItem *item) const;

    /// 在子树里找第一个 MouseArea
    /// @param parent 子树根
    /// @return 命中的 MouseArea；未命中时为 nullptr
    QQuickItem *findChildMouseArea(QQuickItem *parent) const;

    int m_trackTimeout = 100;
    ulong m_eventTimestamp = 0;
    // getText 为 const 方法，但仍需记录实际取值轨道以便诊断
    mutable QString m_lastTrack;
};

#endif // INPUTSIMULATOR_H
