import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Pane {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: "录制"; font.bold: true; font.pixelSize: 18 }

        Connections {
            target: scriptEngine
            onStateChanged: {
                if (state === ScriptEngine.Recording)
                    recordStatus.text = "🎤 录制中..."
                else
                    recordStatus.text = "⏹ 已停止"
            }
        }

        Label {
            id: recordStatus
            text: "⏹ 已停止"
            color: scriptEngine.state === ScriptEngine.Recording ? "red" : "#666"
            font.bold: scriptEngine.state === ScriptEngine.Recording
        }

        ListView {
            id: eventList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel { id: eventModel }
            clip: true
            delegate: ItemDelegate {
                text: model.text || ""
                width: parent.width
            }
        }

        RowLayout {
            Button {
                text: "停止录制"
                highlighted: true
                onClicked: scriptEngine.stopRecording()
            }
            Button {
                text: "保存脚本"
                onClicked: {
                    scriptEngine.saveScript("C:/recorded_script.json")
                }
            }
            Item { Layout.fillWidth: true }
        }
    }
}
