import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

Window {
    id: root
    visible: true
    width: 640
    height: 520
    title: "QtUIAuto Test Target"
    objectName: "testTargetWindow"

    property int clickCount: 0

    Label {
        id: titleLabel
        objectName: "titleLabel"
        text: "QtUIAuto 测试目标应用"
        font.pixelSize: 20
        font.bold: true
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 16
    }

    GroupBox {
        id: buttonGroup
        title: "按钮测试"
        anchors.top: titleLabel.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16

        Button {
            id: btnClickMe
            objectName: "btnClickMe"
            text: "点击我"
            Accessible.role: Accessible.Button
            Accessible.name: "Click Me Button"
            anchors.top: parent.top
            anchors.left: parent.left
            onClicked: {
                root.clickCount++;
                clickResult.text = "按钮被点击了 " + root.clickCount + " 次"
                console.log("[TestTarget] btnClickMe 被点击, 次数:" + root.clickCount)
            }
        }

        Button {
            id: btnRightClick
            objectName: "btnRightClick"
            text: "右键菜单"
            Accessible.role: Accessible.Button
            Accessible.name: "Right Click Button"
            anchors.top: parent.top
            anchors.left: btnClickMe.right
            anchors.leftMargin: 16
            onClicked: {
                clickResult.text = "左键点击了右键按钮"
            }
            onPressed: {
                if (pressedButtons & Qt.RightButton) {
                    clickResult.text = "右键点击了菜单按钮"
                    console.log("[TestTarget] btnRightClick 右键点击")
                }
            }
        }

        Label {
            id: clickResult
            objectName: "clickResultLabel"
            text: "尚未点击"
            color: "#666"
            anchors.top: btnClickMe.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
        }
    }

    GroupBox {
        id: textGroup
        title: "文本输入测试"
        anchors.top: buttonGroup.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16

        TextField {
            id: textInput
            objectName: "textInput"
            placeholderText: "在此输入文本..."
            Accessible.role: Accessible.EditableText
            Accessible.name: "Text Input Field"
            selectByMouse: true
            anchors.top: parent.top
            anchors.left: parent.left
            width: 300
        }

        Button {
            id: btnShowText
            objectName: "btnShowText"
            text: "显示输入内容"
            Accessible.role: Accessible.Button
            Accessible.name: "Show Text Button"
            anchors.top: parent.top
            anchors.left: textInput.right
            anchors.leftMargin: 16
            onClicked: {
                textDisplay.text = "输入内容: " + textInput.text
                console.log("[TestTarget] 显示文本: " + textInput.text)
            }
        }

        Label {
            id: textDisplay
            objectName: "textDisplayLabel"
            text: "等待输入..."
            color: "#666"
            anchors.top: textInput.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
        }
    }

    Label {
        id: statusDisplay
        objectName: "statusDisplay"
        text: "系统就绪"
        font.family: "Courier"
        font.pixelSize: 12
        anchors.bottom: btnExit.top
        anchors.left: parent.left
        anchors.margins: 16
    }

    Button {
        id: btnExit
        objectName: "btnExit"
        text: "退出"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 16
        highlighted: true
        onClicked: Qt.quit()
    }
}
