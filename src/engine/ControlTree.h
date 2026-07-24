#ifndef CONTROLTREE_H
#define CONTROLTREE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

class ControlTree : public QObject
{
    Q_OBJECT
public:
    explicit ControlTree(QObject *parent = nullptr);

    QJsonArray getRootWindowList();
    QJsonObject getControlTree(const QString &windowId = QString());

signals:
    void treeChanged();
};

#endif // CONTROLTREE_H
