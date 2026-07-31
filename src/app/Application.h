#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QQmlApplicationEngine>

class ProcessManager;
class PipeServer;
class ControlTree;
class ScriptEngine;

class Application : public QApplication
{
    Q_OBJECT
public:
    Application(int &argc, char **argv);
    ~Application() override;

private:
    void setupQmlContext();

    QQmlApplicationEngine *m_engine   = nullptr;
    ProcessManager        *m_process  = nullptr;
    PipeServer            *m_pipe     = nullptr;
    ControlTree           *m_tree     = nullptr;
    ScriptEngine          *m_script   = nullptr;
};

#endif // APPLICATION_H
