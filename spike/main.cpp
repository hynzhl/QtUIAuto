#include <QApplication>
#include <QQuickView>
#include <QQmlEngine>
#include <QQmlError>
#include <QDebug>
#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QWindow>
#include <QTimer>
#include <QGuiApplication>
#include <QFile>
#include <QTextStream>
#include <QQuickItem>

static const char *roleToString(QAccessible::Role role) {
    switch (role) {
    case QAccessible::Button:       return "Button";
    case QAccessible::CheckBox:     return "CheckBox";
    case QAccessible::ComboBox:     return "ComboBox";
    case QAccessible::Slider:       return "Slider";
    case QAccessible::StaticText:   return "StaticText";
    case QAccessible::EditableText: return "EditableText";
    case QAccessible::PageTab:      return "PageTab";
    case QAccessible::PageTabList:  return "PageTabList";
    case QAccessible::RadioButton:  return "RadioButton";
    default:                        return "Other";
    }
}

static QQuickItem *findItem(QQuickItem *parent, const QString &name) {
    if (!parent) return nullptr;
    if (parent->objectName() == name) return parent;
    for (auto *c : parent->childItems())
        if (auto *f = findItem(c, name)) return f;
    return nullptr;
}

static void testControl(QTextStream &out, QQuickItem *item, const QString &label) {
    if (!item) { out << "  " << label << ": ITEM NOT FOUND\n"; return; }
    out << "  " << label << ": type=" << item->metaObject()->className()
        << " objName=\"" << item->objectName() << "\"\n";

    QAccessibleInterface *ai = QAccessible::queryAccessibleInterface(item);
    if (!ai) {
        out << "    [Acc] queryAccessibleInterface = NULL\n";
        return;
    }
    QAccessible::Role role = ai->role();
    out << "    [Acc] role=" << roleToString(role) << "(" << role << ")"
        << " name=\"" << ai->text(QAccessible::Name) << "\""
        << " children=" << ai->childCount() << "\n";

    QAccessibleActionInterface *actIface = ai->actionInterface();
    if (actIface) {
        QStringList actions = actIface->actionNames();
        out << "    [Actions]";
        for (auto &a : actions) out << " \"" << a << "\"";
        out << "\n";
        if (actions.contains(QAccessibleActionInterface::pressAction())) {
            out << "    [PressAction] executing BEFORE mouse click...\n";
            actIface->doAction(QAccessibleActionInterface::pressAction());
            out << "    [PressAction] done\n";
        }
    } else {
        out << "    [Actions] NO actionInterface\n";
    }
}

int main(int argc, char *argv[]) {
    qputenv("QT_ACCESSIBILITY", "1");
    QApplication app(argc, argv);
    QQuickView view;
    QObject::connect(view.engine(), &QQmlEngine::warnings,
        [](const QList<QQmlError> &w) {
            for (auto &e : w) qWarning().noquote() << "QML:" << e.toString(); });

    view.setSource(QUrl("qrc:/qml/SpikeWindow.qml"));
    view.setTitle("QtUIAuto Spike - MouseArea Accessibility");
    view.setMinimumSize(QSize(800, 600));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.show();

    QTimer::singleShot(2000, [&]() {
        QString report;
        QTextStream out(&report);
        out << "======== MouseArea Accessibility Test ========\n";
        out << "Qt: " << qVersion() << " QT_ACCESSIBILITY="
            << qEnvironmentVariable("QT_ACCESSIBILITY") << "\n\n";

        QQuickItem *ro = view.rootObject();
        out << "rootObject: " << (ro ? ro->metaObject()->className() : "NULL") << "\n\n";

        // Test each case
        // Case 1: Item + MouseArea WITH Accessible (with onPressAction handler)
        testControl(out, findItem(ro, "customBtnWithAcc"), "Case1: Item+MouseArea WITH Acc+onPressAction");

        // Case 2: Item + MouseArea WITHOUT Accessible at all
        testControl(out, findItem(ro, "customBtnNoAcc"), "Case2: Item+MouseArea NO Acc");

        // Case 3: Item + MouseArea WITH Acc but NO onPressAction handler
        testControl(out, findItem(ro, "customBtnNoHandler"), "Case3: Item+MouseArea Acc NO handler");

        // Case 4: Pure Rectangle + MouseArea (no parent Item wrapper)
        testControl(out, findItem(ro, "pureRect"), "Case4: Rectangle+MouseArea");

        // Case 5: The MouseArea itself (direct test)
        testControl(out, findItem(ro, "pureMouseArea"), "Case5: MouseArea item directly");

        // Summary
        out << "\n--------- Summary ---------\n";
        struct TestCase { QString name; QString objName; };
        QVector<TestCase> cases = {
            {"Item+MouseArea WITH Acc+handler", "customBtnWithAcc"},
            {"Item+MouseArea NO Acc",          "customBtnNoAcc"},
            {"Item+MouseArea Acc NO handler",  "customBtnNoHandler"},
            {"Rectangle+MouseArea",            "pureRect"},
            {"MouseArea directly",             "pureMouseArea"},
        };
        for (auto &tc : cases) {
            auto *item = findItem(ro, tc.objName);
            auto *ai = item ? QAccessible::queryAccessibleInterface(item) : nullptr;
            out << "  " << tc.name << ": "
                << (ai ? "Has Acc" : "NULL    ") << " | objName=" << tc.objName << "\n";
        }
        out << "===========================\n";

        qDebug().noquote() << report;
        QFile f("mousearea_report.txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream fs(&f); fs << report;
        }
        QTimer::singleShot(100, &app, &QApplication::quit);
    });
    return app.exec();
}
