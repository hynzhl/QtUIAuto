#ifndef ACCESSIBILITYCONTROLLER_H
#define ACCESSIBILITYCONTROLLER_H

#include <QObject>
#include <QAccessible>
#include <QAccessibleInterface>
#include <QJsonObject>
#include <QJsonArray>

class AccessibilityController : public QObject
{
    Q_OBJECT
public:
    explicit AccessibilityController(QObject *parent = nullptr);

    // Control tree traversal
    QJsonArray listControls(const QString &rootPath = QString());
    QJsonObject getControlInfo(const QString &path);

    // Actions
    bool click(const QString &path);
    bool typeText(const QString &path, const QString &text);
    bool setFocus(const QString &path);

    // Property access
    QJsonObject getProperty(const QString &path, const QString &property);
    bool setProperty(const QString &path, const QString &property, const QVariant &value);

    // Screenshot
    QString takeScreenshot(const QString &path = QString());

    // Execute a command JSON
    QJsonObject executeCommand(const QJsonObject &cmd);

private:
    QAccessibleInterface *resolvePath(const QString &path);
};

#endif // ACCESSIBILITYCONTROLLER_H
