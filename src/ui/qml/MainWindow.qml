import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/// ============================================================
/// MainWindow — 主界面视图层
///
/// 只与 appContext 门面交互，不持有任何 C++ 引擎对象、不接触文件路径。
/// 状态文案统一由本文件的属性驱动：一旦在 handler 里直接给 Label.text
/// 赋值，声明式绑定会被摧毁，此后连接状态变化就再也刷不到界面上。
/// ============================================================
Pane {
    id: root
    padding: 16

    // ── 属性 ──

    // 瞬时提示（回放进度、脚本读写结果等）。为空时状态栏回落到连接状态展示。
    property string statusMessage: ""

    // 注入失败原因。为空表示尚未失败过。
    property string injectError: ""

    // ═══════════ 子组件 ═══════════

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // ── Header ──
        RowLayout {
            Label {
                text: "QU"
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
                color: appContext.connected ? "green" : "red"
            }
            Label {
                id: statusLabel
                text: root.statusMessage.length > 0
                      ? root.statusMessage
                      : (appContext.connected ? "✅ 已连接" : "❌ 未连接")
                color: root.statusMessage.length > 0
                       ? "#333"
                       : (appContext.connected ? "green" : "red")
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
                        root.injectError = ""
                        appContext.launchTarget(targetPath.text)
                    }
                }
            }
            Button {
                id: injectBtn
                text: root.injectError.length > 0
                      ? "❌ 注入失败: " + root.injectError
                      : (appContext.connected ? "✅ 已注入" : "💉 注入 DLL")
                enabled: !appContext.connected
                onClicked: appContext.injectDll()
            }
            Button {
                text: "⏹ 停止"
                onClicked: appContext.stopTarget()
            }
        }

        // ── 操作按钮栏 ──
        RowLayout {
            spacing: 8
            Button {
                id: recordBtn
                text: appContext.recording ? "⏹ 停止录制" : "▶ 录制"
                onClicked: appContext.toggleRecording()
            }
            Button {
                id: playBtn
                text: appContext.playing ? "⏹ 停止" : "▶ 回放"
                onClicked: appContext.togglePlayback()
            }
            Button {
                text: "📂 打开脚本"
                onClicked: appContext.loadScript()
            }
            Button {
                text: "💾 保存脚本"
                onClicked: appContext.saveScript()
            }
            Button {
                text: "🔄 刷新控件树"
                onClicked: root.refreshTree()
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
                    id: treeView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: ListModel { id: treeModel }
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
                    onClicked: root.refreshScript()
                }
            }

            // ── Tab 2: 报告 ──
            ReportView {}
        }
    }

    // ═══════════ 信号 / Connections ═══════════

    Connections {
        target: appContext

        // Qt 5.15 起隐式 onFoo 属性已弃用，一律用具名函数形式声明处理器。
        function onInjectionResult(success, message) {
            root.injectError = success ? "" : message;
        }

        function onPlaybackStep(step, total) {
            root.statusMessage = "回放中: " + step + "/" + total;
        }

        function onPlaybackFinished(success) {
            root.statusMessage = success ? "✅ 回放完成" : "❌ 回放失败";
        }

        function onScriptSaved(success, path) {
            root.statusMessage = success ? "✅ 脚本已保存: " + path
                                        : "❌ 脚本保存失败: " + path;
        }

        function onScriptLoaded(success, path) {
            if (success) {
                root.statusMessage = "✅ 脚本已加载: " + path;
                root.refreshScript();
            } else {
                root.statusMessage = "❌ 脚本加载失败: " + path;
            }
        }
    }

    // ═══════════ 函数 ═══════════

    /// 重新拉取根窗口列表并填充控件树视图
    function refreshTree() {
        treeModel.clear();
        var windows = appContext.rootWindowList();
        for (var i = 0; i < windows.length; i++) {
            treeModel.append(windows[i]);
        }
    }

    /// 用门面里的当前脚本刷新脚本列表视图
    function refreshScript() {
        scriptModel.clear();
        var steps = appContext.script;
        for (var i = 0; i < steps.length; i++) {
            scriptModel.append(steps[i]);
        }
    }
}
