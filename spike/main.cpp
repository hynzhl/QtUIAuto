#include <QApplication>
#include <QQuickView>
#include <QQmlEngine>
#include <QDebug>
#include <QAccessible>
#include <QWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QtUIAuto_Spike");

    // Enable accessibility
    qputenv("QT_ACCESSIBILITY", "1");

    QQuickView view;
    view.setSource(QUrl("qrc:/qml/SpikeWindow.qml"));
    view.setTitle("QtUIAuto Spike - QAccessibility Verification");
    view.setMinimumSize(QSize(800, 600));
    view.show();

    // Verify accessibility after UI is loaded
    QTimer::singleShot(500, [&]() {
        qInfo() << "\n======= QAccessibility Spike Report =======";

        // 1. Check accessible interfaces on top-level windows
        auto windows = QGuiApplication::topLevelWindows();
        qInfo() << "Top-level windows:" << windows.size();
        for (auto *w : windows) {
            QAccessibleInterface *rootIface = QAccessible::queryAccessibleInterface(w);
            if (rootIface) {
                qInfo() << "  Window:" << w->title()
                        << "| role:" << QAccessible::roleToString(rootIface->role())
                        << "| children:" << rootIface->childCount();
                // Enumerate direct children
                for (int i = 0; i < rootIface->childCount(); ++i) {
                    QAccessibleInterface *child = rootIface->child(i);
                    if (child) {
                        qInfo() << "    [" << i << "]"
                                << "name:" << child->text(QAccessible::Name)
                                << "| role:" << QAccessible::roleToString(child->role())
                                << "| childCount:" << child->childCount();
                        delete child;
                    }
                }
                delete rootIface;
            }
        }

        // 2. Check accessibility for the main QuickView
        QAccessibleInterface *viewIface = QAccessible::queryAccessibleInterface(&view);
        if (viewIface) {
            qInfo() << "\nQuickView accessibility:"
                    << "role:" << QAccessible::roleToString(viewIface->role())
                    << "childCount:" << viewIface->childCount();
            delete viewIface;
        }

        qInfo() << "======= End of Spike Report =======\n";
        qInfo() << "Tip: Run your own QML app with QT_ACCESSIBILITY=1"
                << "and verify control tree traversal.";
    });

    return app.exec();
}
