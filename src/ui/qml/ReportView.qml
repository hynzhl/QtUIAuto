import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Pane {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: "测试报告"; font.bold: true; font.pixelSize: 18 }

        ListView {
            id: reportList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel { id: reportModel }
            clip: true
            delegate: ItemDelegate {
                text: model.text || ""
                width: parent.width
            }
        }

        RowLayout {
            Button { text: "导出报告" }
            Button { text: "清除" }
            Item { Layout.fillWidth: true }
        }
    }
}
