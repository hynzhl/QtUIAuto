import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/// ============================================================
/// RecordPanel — 录制面板视图层
///
/// 说明：本组件目前未被任何界面实例化（录制功能尚未打通，事件列表也
/// 无人填充）。保留它是为了录制链路补齐后能直接接上，但它同样只允许
/// 与 appContext 门面交互，不得引用已下线的引擎上下文属性。
/// ============================================================
Pane {
    id: root

    // ═══════════ 子组件 ═══════════

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: "录制"; font.bold: true; font.pixelSize: 18 }

        Label {
            id: recordStatus
            text: appContext.recording ? "🎤 录制中..." : "⏹ 已停止"
            color: appContext.recording ? "red" : "#666"
            font.bold: appContext.recording
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
                text: appContext.recording ? "停止录制" : "开始录制"
                highlighted: true
                onClicked: appContext.toggleRecording()
            }
            Button {
                text: "保存脚本"
                onClicked: appContext.saveScript()
            }
            Item { Layout.fillWidth: true }
        }
    }
}
