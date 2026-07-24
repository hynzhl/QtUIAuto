#include "Application.h"
#include <QQuickView>
#include <QQmlEngine>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    setApplicationName("QtUIAuto");
    setApplicationVersion("0.1.0");
    setOrganizationName("hynzhl");
}

Application::~Application() = default;
