import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Pane {
    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: "Test Report"; font.bold: true; font.pixelSize: 18 }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: ListModel {}
            delegate: ItemDelegate {
                text: model.text || ""
            }
        }

        RowLayout {
            Button { text: "Export Report" }
            Button { text: "Clear" }
            Item { Layout.fillWidth: true }
        }
    }
}
