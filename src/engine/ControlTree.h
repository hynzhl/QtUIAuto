#ifndef CONTROLTREE_H
#define CONTROLTREE_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class PipeServer;

class ControlTree : public QObject
{
    Q_OBJECT
public:
    explicit ControlTree(PipeServer *pipeServer, QObject *parent = nullptr);

    Q_INVOKABLE QVariantList getRootWindowList();
    Q_INVOKABLE QVariantMap getControlTree(const QString &windowId = QString());

signals:
    void treeChanged();

private:
    PipeServer *m_pipeServer;
};

#endif // CONTROLTREE_H
