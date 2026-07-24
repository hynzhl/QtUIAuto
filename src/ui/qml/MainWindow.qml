import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Pane {
    id: root
    padding: 16

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Header
        Label {
            text: "QtUIAuto"
            font.pixelSize: 24
            font.bold: true
        }

        Label {
            text: "QML UI Automation Testing Tool"
            color: "#666"
        }

        // Action bar
        RowLayout {
            spacing: 8
            Button {
                text: "▶ Record"
                highlighted: true
            }
            Button {
                text: "⏹ Play"
            }
            Button {
                text: "📂 Open Script"
            }
            Item { Layout.fillWidth: true }
        }

        // Main area
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton { text: "Target App" }
            TabButton { text: "Control Tree" }
            TabButton { text: "Script" }
            TabButton { text: "Report" }
        }

        StackLayout {
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item { id: targetPage }
            Item { id: controlTreePage }
            Item { id: scriptPage }
            ReportView { id: reportPage }
        }
    }
}
