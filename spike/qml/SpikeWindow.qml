import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    visible: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            objectName: "spikeTitle"
            text: "MouseArea Accessibility Test"
            font.pixelSize: 18
            font.bold: true
        }

        // === Case 1: Item + MouseArea WITH Accessible ===
        Item {
            objectName: "customBtnWithAcc"
            width: 200; height: 48
            Accessible.role: Accessible.Button
            Accessible.name: "Custom Button WITH Accessible"
            Accessible.onPressAction: {
                console.log("=== ACC via Accessible.onPressAction ===");
                clickCounter.text = "PressAction fired: " + (++counter);
            }

            Rectangle {
                anchors.fill: parent
                color: "#4CAF50"
                radius: 6
                border.color: "#388E3C"

                Text {
                    anchors.centerIn: parent
                    text: "CustomBtn WITH Acc"
                    color: "white"
                }
            }

            MouseArea {
                id: mouseArea1
                anchors.fill: parent
                onClicked: {
                    console.log("=== MouseArea.onClicked ===");
                    clickCounter.text = "Mouse clicked: " + (++counter);
                }
            }

            property int counter: 0
        }

        // === Case 2: Item + MouseArea WITHOUT Accessible ===
        Item {
            objectName: "customBtnNoAcc"
            width: 200; height: 48

            Rectangle {
                anchors.fill: parent
                color: "#9E9E9E"
                radius: 6
                border.color: "#757575"

                Text {
                    anchors.centerIn: parent
                    text: "CustomBtn NO Acc"
                    color: "white"
                }
            }

            MouseArea {
                id: mouseArea2
                anchors.fill: parent
                onClicked: {
                    console.log("=== MouseArea.onClicked (no acc) ===");
                    clickCounter.text = "Mouse clicked (no acc): " + (++counter);
                }
            }

            property int counter: 0
        }

        // === Case 3: Rect + MouseArea + Acc but NO onPressAction handler ===
        Item {
            objectName: "customBtnNoHandler"
            width: 200; height: 48
            Accessible.role: Accessible.Button
            Accessible.name: "Custom Button without onPressAction"

            Rectangle {
                anchors.fill: parent
                color: "#FF9800"
                radius: 6
                border.color: "#F57C00"

                Text {
                    anchors.centerIn: parent
                    text: "Btn Acc NO handler"
                    color: "white"
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    console.log("=== MouseArea.onClicked (no handler) ===");
                    clickCounter.text = "Mouse clicked (no handler): " + (++counter);
                }
            }

            property int counter: 0
        }

        // === Case 4: Pure MouseArea (without parent Item wrapper) ===
        Rectangle {
            objectName: "pureRect"
            width: 200; height: 48
            color: "#2196F3"
            radius: 6

            Text {
                anchors.centerIn: parent
                text: "Pure Rect+MouseArea"
                color: "white"
            }

            MouseArea {
                objectName: "pureMouseArea"
                anchors.fill: parent
                onClicked: {
                    console.log("=== Pure MouseArea onClicked ===");
                    clickCounter.text = "Pure MouseArea: " + (++counter);
                }
            }

            property int counter: 0
        }

        // Status display
        Label {
            id: clickCounter
            objectName: "clickCounter"
            text: "No clicks yet"
            color: "#666"
            font.italic: true
        }

        Label {
            text: "\nCheck console for accessibility details."
            color: "#888"
            font.italic: true
        }
    }
}
