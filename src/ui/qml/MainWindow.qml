import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Pane {
    id: root
    padding: 16

    // ── 信号连接（通过 Connections 绑定 C++ 信号）──
    Connections {
        target: pipeServer
        onInjectReady: {
            statusLabel.text = "✅ 已连接"
            statusLabel.color = "green"
        }
        onClientDisconnected: {
            statusLabel.text = "❌ 未连接"
            statusLabel.color = "red"
        }
    }

    Connections {
        target: processManager
        onInjectionResult: {
            if (success) {
                injectBtn.text = "✅ 已注入"
                injectBtn.enabled = false
            } else {
                injectBtn.text = "❌ 注入失败: " + message
                injectBtn.enabled = true
            }
        }
    }

    Connections {
        target: scriptEngine
        onStateChanged: {
            recordBtn.text = (state === ScriptEngine.Recording) ? "⏹ 停止录制" : "▶ 录制"
            playBtn.text   = (state === ScriptEngine.Playing)  ? "⏹ 停止" : "▶ 回放"
        }
        onPlaybackStep: {
            statusLabel.text = "回放中: " + step + "/" + total
        }
        onPlaybackFinished: {
            statusLabel.text = success ? "✅ 回放完成" : "❌ 回放失败"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // ── Header ──
        RowLayout {
            Label {
                text: "QtUIAuto"
                font.pixelSize: 24
                font.bold: true
            }
            Label {
                text: "v0.1.0"
                color: "#999"
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                width: 12; height: 12; radius: 6
                color: pipeServer.connected ? "green" : "red"
            }
            Label {
                id: statusLabel
                text: pipeServer.connected ? "✅ 已连接" : "❌ 未连接"
                color: pipeServer.connected ? "green" : "red"
            }
        }

        // ── 目标进程控制栏 ──
        RowLayout {
            spacing: 8
            TextField {
                id: targetPath
                Layout.fillWidth: true
                placeholderText: "目标应用路径 (exe)..."
                selectByMouse: true
            }
            Button {
                text: "🚀 启动目标"
                highlighted: true
                onClicked: {
                    if (targetPath.text.length > 0) {
                        processManager.launchTarget(targetPath.text)
                    }
                }
            }
            Button {
                id: injectBtn
                text: "💉 注入 DLL"
                onClicked: processManager.injectDll()
            }
            Button {
                text: "⏹ 停止"
                onClicked: processManager.stopTarget()
            }
        }

        // ── 操作按钮栏 ──
        RowLayout {
            spacing: 8
            Button {
                id: recordBtn
                text: "▶ 录制"
                onClicked: {
                    if (scriptEngine.state === ScriptEngine.Recording)
                        scriptEngine.stopRecording()
                    else
                        scriptEngine.startRecording()
                }
            }
            Button {
                id: playBtn
                text: "▶ 回放"
                onClicked: {
                    if (scriptEngine.state === ScriptEngine.Playing)
                        scriptEngine.stopPlayback()
                    else
                        scriptEngine.startPlayback()
                }
            }
            Button {
                text: "📂 打开脚本"
                onClicked: {
                    scriptEngine.loadScript("C:/test_script.json")
                    statusLabel.text = "脚本已加载"
                }
            }
            Button {
                text: "💾 保存脚本"
                onClicked: {
                    scriptEngine.saveScript("C:/recorded_script.json")
                    statusLabel.text = "脚本已保存"
                }
            }
            Button {
                text: "🔄 刷新控件树"
                onClicked: {
                    treeModel.model.clear()
                    var windows = controlTree.getRootWindowList()
                    for (var i = 0; i < windows.length; i++) {
                        treeModel.model.append(windows[i])
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ── 主区域 Tab ──
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            TabButton { text: "控件树" }
            TabButton { text: "脚本" }
            TabButton { text: "报告" }
        }

        StackLayout {
            currentIndex: tabBar.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ── Tab 0: 控件树 ──
            ColumnLayout {
                ListView {
                    id: treeModel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: ListModel {}
                    clip: true
                    delegate: ItemDelegate {
                        text: "[" + model.objectName + "] " + model.typeName
                        width: parent.width
                    }
                }
            }

            // ── Tab 1: 脚本 ──
            ColumnLayout {
                ListView {
                    id: scriptView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: ListModel { id: scriptModel }
                    clip: true
                    delegate: ItemDelegate {
                        text: "[" + (index + 1) + "] " + model.action
                        width: parent.width
                    }
                }
                Button {
                    text: "🔄 刷新脚本列表"
                    onClicked: {
                        scriptModel.clear()
                        var steps = scriptEngine.script
                        for (var i = 0; i < steps.length; i++) {
                            scriptModel.append(steps[i])
                        }
                    }
                }
            }

            // ── Tab 2: 报告 ──
            ReportView {}
        }
    }
}

