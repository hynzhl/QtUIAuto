#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/TestTarget.qml")));

    if (engine.rootObjects().isEmpty())
    {
        qCritical() << "[TestTarget] QML load failed";
        return 1;
    }

    qInfo() << "[TestTarget] Started, PID:" << QGuiApplication::applicationPid();
    return app.exec();
}
