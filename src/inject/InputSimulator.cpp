#include "InputSimulator.h"
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QScopedPointer>
#include <QAccessibleValueInterface>

InputSimulator::InputSimulator(QObject *parent)
    : QObject(parent)
{
}

// ═══════════════════════ 公有接口 ═══════════════════════

bool InputSimulator::click(QQuickItem *target)
{
    if (!target)
    {
        qWarning() << "[InputSimulator] click: target 为空";
        return false;
    }

    // Track 1: 窗口场景投递。不能把 Accessibility 放在最前：
    // QAccessibleQuickItem::actionNames() 只要 role 是 Button 就会返回 pressAction，
    // 但 doAction 仅 emit QQuickAccessibleAttached::pressAction() 信号；
    // 目标 QML 未写 Accessible.onPressAction 时信号无接收者，
    // 于是“动作可用”但实际静默无效，会把未生效的点击误报为成功。
    if (tryWindowDeliveryClick(target))
    {
        m_lastTrack = QStringLiteral("Window::sendEvent");
        return true;
    }

    if (tryMouseAreaClick(target))
    {
        m_lastTrack = QStringLiteral("MouseArea::sendEvent");
        return true;
    }

    if (trySendEventClick(target))
    {
        m_lastTrack = QStringLiteral("Item::sendEvent");
        return true;
    }

    // Track 4: 仅当事件路径全部不可用时才用 Accessibility
    if (tryAccessibleClick(target))
    {
        m_lastTrack = QStringLiteral("Accessibility::doAction");
        return true;
    }

    qWarning() << "[InputSimulator] click: 所有降级路径均失败";
    return false;
}

bool InputSimulator::doubleClick(QQuickItem *target)
{
    if (!target)
    {
        qWarning() << "[InputSimulator] doubleClick: target 为空";
        return false;
    }

    // Track 1: 窗口场景投递，按 QTest 的 press → release → dblclick → release 序列
    if (target->window())
    {
        const bool accepted =
            sendMouseEventViaWindow(target, QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
        sendMouseEventViaWindow(target, QEvent::MouseButtonRelease, Qt::LeftButton, Qt::MouseButtons());
        sendMouseEventViaWindow(target, QEvent::MouseButtonDblClick, Qt::LeftButton, Qt::LeftButton);
        sendMouseEventViaWindow(target, QEvent::MouseButtonRelease, Qt::LeftButton, Qt::MouseButtons());
        if (accepted)
        {
            m_lastTrack = QStringLiteral("Window::doubleClick");
            return true;
        }
    }

    // Track 2/3: 先查找子 MouseArea，否则直接投递给 item
    QQuickItem *mouseArea = findChildMouseArea(target);
    QQuickItem *receiver = mouseArea ? mouseArea : target;

    QPointF center = itemCenter(receiver);
    bool ok = true;
    ok &= sendMousePressAt(receiver, center);
    ok &= sendMouseDoubleClickAt(receiver, center);
    ok &= sendMouseReleaseAt(receiver, center);

    m_lastTrack = mouseArea
        ? QStringLiteral("MouseArea::doubleClick")
        : QStringLiteral("Item::doubleClick");
    return ok;
}

bool InputSimulator::rightClick(QQuickItem *target)
{
    if (!target) return false;

    if (target->window())
    {
        const bool accepted =
            sendMouseEventViaWindow(target, QEvent::MouseButtonPress, Qt::RightButton, Qt::RightButton);
        sendMouseEventViaWindow(target, QEvent::MouseButtonRelease, Qt::RightButton, Qt::MouseButtons());
        if (accepted)
        {
            m_lastTrack = QStringLiteral("Window::rightClick");
            return true;
        }
    }

    QPointF center = itemCenter(target);
    bool ok = true;
    ok &= sendMousePressAt(target, center, Qt::RightButton);
    ok &= sendMouseReleaseAt(target, center, Qt::RightButton);

    m_lastTrack = QStringLiteral("rightClick::sendEvent");
    return ok;
}

bool InputSimulator::typeText(QQuickItem *target, const QString &text)
{
    if (!target || text.isEmpty())
    {
        qWarning() << "[InputSimulator] typeText: 无效参数";
        return false;
    }

    // 先尝试设焦
    setFocus(target);

    // Track 2: setProperty("text")
    if (tryPropertySetText(target, text))
    {
        m_lastTrack = QStringLiteral("property::setText");
        return true;
    }

    // Track 3: 逐个字符发送 key event
    if (trySendEventTypeText(target, text))
    {
        m_lastTrack = QStringLiteral("sendEvent::typeText");
        return true;
    }

    qWarning() << "[InputSimulator] typeText: 所有降级路径均失败";
    return false;
}

bool InputSimulator::setFocus(QQuickItem *target)
{
    if (!target) return false;

    if (tryAccessibleSetFocus(target))
    {
        m_lastTrack = QStringLiteral("Accessibility::setFocus");
        return true;
    }

    target->forceActiveFocus();
    m_lastTrack = QStringLiteral("Item::forceActiveFocus");
    return true;
}

QVariant InputSimulator::getText(QQuickItem *target) const
{
    if (!target) return QVariant();

    // 内容属性优先：Accessible.name 是控件标识（如 "Text Input Field"），
    // 而非控件当前内容；若优先取它，读回的值会与界面实际显示不一致。
    QVariant v = target->property("text");
    if (v.isValid())
    {
        m_lastTrack = QStringLiteral("property::text");
        return v;
    }

    v = target->property("displayText");
    if (v.isValid())
    {
        m_lastTrack = QStringLiteral("property::displayText");
        return v;
    }

    v = target->property("label");
    if (v.isValid())
    {
        m_lastTrack = QStringLiteral("property::label");
        return v;
    }

    // 回退：无内容属性的自定义控件才依赖 Accessibility 描述
    QAccessibleInterface *ai = QAccessible::queryAccessibleInterface(target);
    if (ai)
    {
        QString name = ai->text(QAccessible::Name);
        if (!name.isEmpty())
        {
            m_lastTrack = QStringLiteral("Accessibility::name");
            return name;
        }
        QString desc = ai->text(QAccessible::Description);
        if (!desc.isEmpty())
        {
            m_lastTrack = QStringLiteral("Accessibility::description");
            return desc;
        }
    }

    m_lastTrack = QStringLiteral("none");
    return QVariant();
}

bool InputSimulator::setValue(QQuickItem *target, const QString &property,
                               const QVariant &value)
{
    if (!target) return false;

    QAccessibleInterface *ai = QAccessible::queryAccessibleInterface(target);
    if (ai)
    {
        QAccessibleValueInterface *valIface = ai->valueInterface();
        if (valIface)
        {
            valIface->setCurrentValue(value);
            m_lastTrack = QStringLiteral("Accessibility::setValue");
            return true;
        }
    }

    bool ok = target->setProperty(property.toUtf8().constData(), value);
    if (ok)
    {
        m_lastTrack = QStringLiteral("property::setValue");
    }
    return ok;
}

void InputSimulator::setTrackTimeout(int ms)
{
    m_trackTimeout = qMax(0, ms);
}

// ═══════════════════════ Track 1: 窗口场景投递 ═══════════════════════

// 把合成鼠标事件投递给 QQuickWindow（而不是 item），坐标用 scene 位置，
// 由 Qt Quick 自行完成命中测试、grab 管理与分发——等同于 QTest::mouseClick 的做法。
// 直接向 QQuickItem sendEvent 会绕过这整套分发机制，很多控件根本不会响应。
bool InputSimulator::sendMouseEventViaWindow(QQuickItem *target, QEvent::Type type,
                                             Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QQuickWindow *window = target->window();
    if (!window) return false;

    const QPointF center    = itemCenter(target);
    const QPointF scenePos  = target->mapToScene(center);
    const QPointF screenPos = target->mapToGlobal(center);

    QMouseEvent event(type, scenePos, scenePos, screenPos,
                      button, buttons, Qt::NoModifier);
    m_eventTimestamp += 10;
    event.setTimestamp(m_eventTimestamp);
    event.setAccepted(false);

    QCoreApplication::sendEvent(window, &event);
    return event.isAccepted();
}

bool InputSimulator::tryWindowDeliveryClick(QQuickItem *target)
{
    if (!target->window()) return false;

    // 以 press 是否被接受作为命中判据；release 无论如何都要发，否则控件会停在按下态
    const bool pressAccepted =
        sendMouseEventViaWindow(target, QEvent::MouseButtonPress, Qt::LeftButton, Qt::LeftButton);
    sendMouseEventViaWindow(target, QEvent::MouseButtonRelease, Qt::LeftButton, Qt::MouseButtons());

    if (pressAccepted)
        qDebug() << "[InputSimulator] 使用 Window::sendEvent 点击";
    else
        qWarning() << "[InputSimulator] Window 投递未被接受，降级下一轨道";
    return pressAccepted;
}

// ═══════════════════════ Track 4: Accessibility ═══════════════════════

bool InputSimulator::tryAccessibleClick(QQuickItem *target)
{
    QAccessibleInterface *ai = QAccessible::queryAccessibleInterface(target);
    if (!ai) return false;

    QAccessibleActionInterface *actIface = ai->actionInterface();
    if (!actIface) return false;

    const QStringList actions = actIface->actionNames();
    if (!actions.contains(QAccessibleActionInterface::pressAction()))
    {
        return false;
    }

    actIface->doAction(QAccessibleActionInterface::pressAction());
    qDebug() << "[InputSimulator] 使用 Accessibility::doAction(Press)";
    return true;
}

bool InputSimulator::tryAccessibleSetFocus(QQuickItem *target)
{
    QAccessibleInterface *ai = QAccessible::queryAccessibleInterface(target);
    if (!ai) return false;

    QAccessibleActionInterface *actIface = ai->actionInterface();
    if (!actIface) return false;

    const QStringList actions = actIface->actionNames();
    if (!actions.contains(QAccessibleActionInterface::setFocusAction()))
    {
        return false;
    }

    actIface->doAction(QAccessibleActionInterface::setFocusAction());
    qDebug() << "[InputSimulator] 使用 Accessibility::doAction(SetFocus)";
    return true;
}

bool InputSimulator::tryAccessibleTypeText(QQuickItem *target,
                                            const QString &text)
{
    Q_UNUSED(text)
    return tryAccessibleSetFocus(target);
}

// ═══════════════════════ Track 2: MouseArea 定位 ═══════════════════════

bool InputSimulator::tryMouseAreaClick(QQuickItem *target)
{
    QQuickItem *mouseArea = findChildMouseArea(target);
    if (!mouseArea) return false;

    QPointF localCenter(itemCenter(mouseArea));
    bool ok = true;
    ok &= sendMousePressAt(mouseArea, localCenter);
    ok &= sendMouseReleaseAt(mouseArea, localCenter);

    if (ok) qDebug() << "[InputSimulator] 使用 MouseArea::sendEvent 点击";
    return ok;
}

// ═══════════════════════ Track 3: sendEvent 通用回退 ═══════════════════════

bool InputSimulator::trySendEventClick(QQuickItem *target)
{
    QPointF center = itemCenter(target);
    bool ok = true;
    ok &= sendMousePressAt(target, center);
    ok &= sendMouseReleaseAt(target, center);

    if (ok) qDebug() << "[InputSimulator] 使用 Item::sendEvent 点击";
    return ok;
}

bool InputSimulator::trySendEventTypeText(QQuickItem *target,
                                           const QString &text)
{
    setFocus(target);

    bool ok = true;
    for (const QChar &ch : text)
    {
        ok &= sendKeyPress(target, ch.unicode(), Qt::NoModifier);
        ok &= sendKeyRelease(target, ch.unicode(), Qt::NoModifier);
    }
    return ok;
}

bool InputSimulator::tryPropertySetText(QQuickItem *target,
                                         const QString &text)
{
    if (target->setProperty("text", text)) return true;
    if (target->setProperty("displayText", text)) return true;
    return false;
}

// ═══════════════════════ 事件构造 ═══════════════════════

bool InputSimulator::sendMousePressAt(QQuickItem *receiver,
                                       const QPointF &localPos,
                                       Qt::MouseButton button)
{
    QPointF global = receiver->mapToGlobal(localPos);
    QScopedPointer<QMouseEvent> event(new QMouseEvent(
        QEvent::MouseButtonPress, localPos, global,
        button, button, Qt::NoModifier));
    return QCoreApplication::sendEvent(receiver, event.data());
}

bool InputSimulator::sendMouseReleaseAt(QQuickItem *receiver,
                                         const QPointF &localPos,
                                         Qt::MouseButton button)
{
    QPointF global = receiver->mapToGlobal(localPos);
    QScopedPointer<QMouseEvent> event(new QMouseEvent(
        QEvent::MouseButtonRelease, localPos, global,
        button, Qt::MouseButtons(), Qt::NoModifier));
    return QCoreApplication::sendEvent(receiver, event.data());
}

bool InputSimulator::sendMouseDoubleClickAt(QQuickItem *receiver,
                                             const QPointF &localPos,
                                             Qt::MouseButton button)
{
    QPointF global = receiver->mapToGlobal(localPos);
    QScopedPointer<QMouseEvent> event(new QMouseEvent(
        QEvent::MouseButtonDblClick, localPos, global,
        button, button, Qt::NoModifier));
    return QCoreApplication::sendEvent(receiver, event.data());
}

bool InputSimulator::sendKeyPress(QQuickItem *receiver, int key,
                                   Qt::KeyboardModifiers mods)
{
    QString text;
    if (QChar(key).isPrint())
    {
        text = QString(QChar(key));
    }
    QScopedPointer<QKeyEvent> event(
        new QKeyEvent(QEvent::KeyPress, key, mods, text));
    return QCoreApplication::sendEvent(receiver, event.data());
}

bool InputSimulator::sendKeyRelease(QQuickItem *receiver, int key,
                                     Qt::KeyboardModifiers mods)
{
    QString text;
    if (QChar(key).isPrint())
    {
        text = QString(QChar(key));
    }
    QScopedPointer<QKeyEvent> event(
        new QKeyEvent(QEvent::KeyRelease, key, mods, text));
    return QCoreApplication::sendEvent(receiver, event.data());
}

// ═══════════════════════ 辅助 ═══════════════════════

QPointF InputSimulator::itemCenter(QQuickItem *item) const
{
    return QPointF(item->width() / 2.0, item->height() / 2.0);
}

QPointF InputSimulator::itemGlobalCenter(QQuickItem *item) const
{
    return item->mapToGlobal(itemCenter(item));
}

QQuickItem *InputSimulator::findChildMouseArea(QQuickItem *parent) const
{
    if (!parent) return nullptr;

    const auto children = parent->childItems();
    for (auto *child : children)
    {
        if (!child) continue;

        // 使用 inherits 而非 className 匹配，避免对内部命名格式的依赖
        // QQuickMouseArea 是 Qt Quick 内部类，但 inherits 无需私有头文件
        if (child->inherits("QQuickMouseArea"))
        {
            return child;
        }
    }
    return nullptr;
}
