import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Pane {
    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: "Recording..."
            color: "red"
            font.bold: true
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel {}
            delegate: ItemDelegate {
                text: model.text || ""
            }
        }

        RowLayout {
            Button { text: "Stop Recording"; highlighted: true }
            Button { text: "Save Script" }
            Item { Layout.fillWidth: true }
        }
    }
}
