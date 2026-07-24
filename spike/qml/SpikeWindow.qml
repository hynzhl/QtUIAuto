import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15

Window {
    id: root
    visible: true
    width: 600
    height: 500
    title: "QtUIAuto Spike - Test QML Controls"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Label {
            text: "QAccessibility Verification"
            font.pixelSize: 18
            font.bold: true
        }

        Label {
            text: "This window contains standard QML controls for" +
                  " QAccessibility API verification."
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Standard controls
        Button {
            objectName: "spikeButton"
            text: "Click Me"
            Layout.alignment: Qt.AlignHCenter
        }

        TextField {
            objectName: "spikeTextField"
            placeholderText: "Enter text here..."
            Layout.fillWidth: true
        }

        ComboBox {
            objectName: "spikeCombo"
            model: ["Option A", "Option B", "Option C"]
            Layout.fillWidth: true
        }

        CheckBox {
            objectName: "spikeCheckbox"
            text: "Enable Feature"
        }

        Slider {
            objectName: "spikeSlider"
            from: 0; to: 100; value: 50
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            RadioButton { objectName: "radio1"; text: "Choice 1" }
            RadioButton { objectName: "radio2"; text: "Choice 2" }
        }

        TabBar {
            objectName: "spikeTabBar"
            Layout.fillWidth: true
            TabButton { text: "Tab A" }
            TabButton { text: "Tab B" }
        }

        Label {
            text: "\nCheck the console output for accessibility info."
            color: "#888"
            font.italic: true
        }
    }
}
