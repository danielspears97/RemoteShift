import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs

import org.streetpea.chiaking

// RemoteShift keyboard & mouse hub: profiles, keybind editor, mouse tuning, live test, settings.
Page {
    id: view
    property var store: Chiaki.remoteShift
    property int page: 0
    property string mode: ""
    readonly property var modeNames: { store.revision; return store.modes(); }
    signal closeRequested()

    RsStyle { id: st }
    font.pixelSize: st.fontBody

    component Readout: RowLayout {
        id: ro
        property string name
        property string value
        Label { text: ro.name; color: st.textDim; Layout.fillWidth: true }
        Label { text: ro.value; color: st.text; font.family: "monospace" }
    }
    Material.theme: Material.Dark
    Material.accent: st.accent
    background: Rectangle { color: st.bg }

    function ensureMode() {
        if (modeNames.indexOf(mode) < 0)
            mode = store.defaultMode() || (modeNames.length ? modeNames[0] : "");
    }
    onModeNamesChanged: ensureMode()
    Component.onCompleted: ensureMode()
    Component.onDestruction: store.stopPreview()

    Keys.onEscapePressed: if (!store.previewActive) view.closeRequested()

    // ------------------------------------------------------------ shared dialogs
    Popup {
        id: namePrompt
        property string title
        property string initial
        property var callback
        property string error
        modal: true
        Overlay.modal: Rectangle { color: "#b0000000" }
        anchors.centerIn: Overlay.overlay
        width: 440
        padding: 22
        background: Rectangle { radius: 14; color: st.panel; border.color: st.border }
        function ask(t, init, cb) {
            title = t; initial = init; callback = cb; error = "";
            nameField.text = init;
            open();
            nameField.forceActiveFocus();
            nameField.selectAll();
        }
        function accept() {
            const err = callback(nameField.text);
            if (err && err.length)
                error = err;
            else
                close();
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: namePrompt.title; font.pixelSize: st.fontTitle; font.bold: true; color: st.text }
            TextField {
                id: nameField
                Layout.fillWidth: true
                onAccepted: namePrompt.accept()
            }
            Label { visible: namePrompt.error.length > 0; text: namePrompt.error; color: st.bad; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                RsButton { text: qsTr("Cancel"); flat: true; onClicked: namePrompt.close() }
                RsButton { text: qsTr("OK"); highlighted: true; onClicked: namePrompt.accept() }
            }
        }
    }

    Popup {
        id: confirm
        property string title
        property string text
        property var callback
        modal: true
        Overlay.modal: Rectangle { color: "#b0000000" }
        anchors.centerIn: Overlay.overlay
        width: 440
        padding: 22
        background: Rectangle { radius: 14; color: st.panel; border.color: st.border }
        function ask(t, msg, cb) { title = t; text = msg; callback = cb; open(); }
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: confirm.title; font.pixelSize: st.fontTitle; font.bold: true; color: st.text }
            Label { text: confirm.text; color: st.textDim; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Item { Layout.fillWidth: true }
                RsButton { text: qsTr("Cancel"); flat: true; onClicked: confirm.close() }
                RsButton { text: qsTr("Confirm"); highlighted: true; Material.accent: st.bad; onClicked: { confirm.close(); confirm.callback(); } }
            }
        }
    }

    property string toastText: ""
    function toast(msg) { toastText = msg; toastTimer.restart(); }
    Timer { id: toastTimer; interval: 3500; onTriggered: view.toastText = "" }

    FileDialog {
        id: importDialog
        title: qsTr("Import input profile")
        nameFilters: [qsTr("RemoteShift profiles (*.json)")]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            const err = store.importProfile(selectedFile);
            view.toast(err.length ? err : qsTr("Imported profile \"%1\"").arg(store.activeProfile));
        }
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export input profile")
        nameFilters: [qsTr("RemoteShift profiles (*.json)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        onAccepted: {
            const err = store.exportProfile(store.activeProfile, selectedFile);
            view.toast(err.length ? err : qsTr("Exported to %1").arg(selectedFile));
        }
    }

    RsBindingEditor {
        id: bindingEditor
        objectName: "bindingEditorForHarness"
        store: view.store
        mode: view.mode
        modeNames: view.modeNames
    }

    // ------------------------------------------------------------ layout
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 236
            color: st.panel
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                RowLayout {
                    spacing: 10
                    Layout.bottomMargin: 14
                    Image {
                        Layout.preferredWidth: 36
                        Layout.preferredHeight: 36
                        sourceSize: Qt.size(72, 72)
                        source: "qrc:/icons/remoteshift.svg"
                    }
                    ColumnLayout {
                        spacing: 0
                        Label { text: "RemoteShift"; font.pixelSize: 17; font.bold: true; color: st.text }
                        Label { text: qsTr("Keyboard & mouse"); font.pixelSize: st.fontSmall; color: st.textDim }
                    }
                }

                Repeater {
                    model: [qsTr("Input Profiles"), qsTr("Keybind Editor"), qsTr("Mouse && Movement"), qsTr("Live Input"), qsTr("Input Settings")]
                    delegate: ItemDelegate {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        height: 44
                        text: modelData
                        highlighted: view.page === index
                        onClicked: view.page = index
                        background: Rectangle {
                            radius: 8
                            color: parent.highlighted ? st.accentSoft : (parent.hovered ? st.panel2 : "transparent")
                            Rectangle {
                                visible: parent.parent.highlighted
                                width: 3; radius: 2
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
                                color: st.accent
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: activeCol.implicitHeight + 20
                    radius: st.radius
                    color: st.panel2
                    ColumnLayout {
                        id: activeCol
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                        spacing: 2
                        Label { text: qsTr("Active profile"); color: st.textFaint; font.pixelSize: st.fontSmall }
                        Label { text: store.activeProfile; color: st.text; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label {
                            text: store.sessionActive ? (store.live.captured ? qsTr("● Streaming · mouse captured") : qsTr("● Streaming · click stream to capture"))
                                                      : qsTr("Not streaming")
                            color: store.sessionActive ? st.good : st.textFaint
                            font.pixelSize: st.fontSmall
                        }
                    }
                }
                RsButton {
                    Layout.fillWidth: true
                    text: qsTr("Back")
                    flat: true
                    onClicked: view.closeRequested()
                }
            }
        }

        // Content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: view.page

            // ==================================================== 0: Profiles
            ScrollView {
                id: profilesPage
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: profilesPage.availableWidth
                    spacing: 16
                    Item { height: 8 }
                    RowLayout {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        ColumnLayout {
                            spacing: 2
                            Label { text: qsTr("Input Profiles"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                            Label { text: qsTr("Per-game keyboard & mouse layouts. The active profile is used for every stream."); color: st.textDim }
                        }
                        Item { Layout.fillWidth: true }
                        RsButton { text: qsTr("Import…"); flat: true; onClicked: importDialog.open() }
                        RsButton {
                            text: qsTr("New profile")
                            highlighted: true
                            onClicked: namePrompt.ask(qsTr("New profile"), "", (n) => store.createProfile(n, ""))
                        }
                    }

                    GridLayout {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        Layout.fillWidth: true
                        columns: Math.max(1, Math.floor((profilesPage.availableWidth - 56) / 300))
                        columnSpacing: 14
                        rowSpacing: 14
                        Repeater {
                            model: store.profiles
                            delegate: Rectangle {
                                required property string modelData
                                readonly property var info: { store.revision; return store.profileInfo(modelData); }
                                readonly property bool isActive: store.activeProfile === modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 128
                                radius: 12
                                color: isActive ? st.accentSoft : st.panel
                                border.color: isActive ? st.accent : (cardMouse.containsMouse ? st.panel3 : st.border)
                                border.width: isActive ? 2 : 1
                                MouseArea {
                                    id: cardMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: store.activeProfile = parent.modelData
                                }
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 4
                                    RowLayout {
                                        Label { text: modelData; font.pixelSize: 18; font.bold: true; color: st.text; elide: Text.ElideRight; Layout.fillWidth: true }
                                        Rectangle {
                                            visible: isActive
                                            radius: 9; height: 20; width: activeTag.implicitWidth + 14
                                            color: st.accent
                                            Label { id: activeTag; anchors.centerIn: parent; text: qsTr("ACTIVE"); font.pixelSize: 10; font.bold: true; color: "white" }
                                        }
                                    }
                                    Label { text: info.game || qsTr("Any game"); color: st.textDim; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Item { Layout.fillHeight: true }
                                    Label { text: qsTr("%1 modes · %2 bindings").arg(info.modes).arg(info.bindings); color: st.textFaint; font.pixelSize: st.fontSmall }
                                }
                            }
                        }
                    }

                    RsCard {
                        id: activeCard
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        Layout.fillWidth: true
                        title: store.activeProfile
                        subtitle: qsTr("Stored in %1").arg(store.profileDirectory)
                        readonly property var info: { store.revision; return store.profileInfo(store.activeProfile); }

                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            Layout.fillWidth: true
                            Label { text: qsTr("Game"); color: st.textDim }
                            TextField {
                                Layout.fillWidth: true
                                text: activeCard.info.game || ""
                                placeholderText: qsTr("e.g. Grand Theft Auto VI")
                                onEditingFinished: if (text !== (activeCard.info.game || "")) store.setProfileInfo("game", text)
                            }
                            Label { text: qsTr("Notes"); color: st.textDim; Layout.alignment: Qt.AlignTop; Layout.topMargin: 12 }
                            TextArea {
                                Layout.fillWidth: true
                                Layout.minimumHeight: 80
                                wrapMode: TextArea.Wrap
                                text: activeCard.info.description || ""
                                onEditingFinished: if (text !== (activeCard.info.description || "")) store.setProfileInfo("description", text)
                            }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            RsButton { text: qsTr("Rename"); onClicked: namePrompt.ask(qsTr("Rename profile"), store.activeProfile, (n) => store.renameProfile(store.activeProfile, n)) }
                            RsButton { text: qsTr("Duplicate"); onClicked: namePrompt.ask(qsTr("Duplicate profile"), store.activeProfile + " copy", (n) => store.createProfile(n, store.activeProfile)) }
                            RsButton { text: qsTr("Export…"); onClicked: { exportDialog.currentFile = "file:///" + store.activeProfile + ".json"; exportDialog.open(); } }
                            RsButton { text: qsTr("Edit keybinds"); highlighted: true; onClicked: view.page = 1 }
                            RsButton {
                                text: qsTr("Delete")
                                flat: true
                                danger: true
                                enabled: store.profiles.length > 1
                                onClicked: confirm.ask(qsTr("Delete profile"), qsTr("Delete \"%1\"? This removes its file from disk.").arg(store.activeProfile), () => {
                                    const err = store.deleteProfile(store.activeProfile);
                                    if (err.length) view.toast(err);
                                })
                            }
                            RsButton {
                                text: qsTr("Restore bundled profiles")
                                flat: true
                                onClicked: confirm.ask(qsTr("Restore bundled profiles"), qsTr("Reset GTA 6 and Default Third-Person to their original bindings? Your other profiles are not touched."), () => { store.restoreBundledProfiles(); view.toast(qsTr("Bundled profiles restored")); })
                            }
                        }
                        Repeater {
                            model: { store.revision; return store.validate(); }
                            delegate: Label {
                                required property string modelData
                                text: "⚠ " + modelData
                                color: st.warn
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                    Item { height: 20 }
                }
            }

            // ==================================================== 1: Keybind editor
            ColumnLayout {
                spacing: 12
                Item { height: 8 }
                RowLayout {
                    Layout.leftMargin: 28; Layout.rightMargin: 28
                    ColumnLayout {
                        spacing: 2
                        Label { text: qsTr("Keybind Editor"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                        Label { text: qsTr("%1 · click a controller button to see or add its bindings").arg(store.activeProfile); color: st.textDim }
                    }
                    Item { Layout.fillWidth: true }
                    RsButton { text: qsTr("Add binding"); highlighted: true; onClicked: bindingEditor.openNew(diagram.selected && store.buttonNames.indexOf(diagram.selected) >= 0 ? { action: "Button", button: diagram.selected } : (diagram.selected && store.actionTypes.indexOf(diagram.selected) >= 0 ? { action: diagram.selected } : null)) }
                }

                RowLayout {
                    Layout.leftMargin: 28; Layout.rightMargin: 28
                    Layout.fillWidth: true
                    spacing: 6
                    TabBar {
                        id: modeTabs
                        Layout.fillWidth: true
                        background: Item {}
                        currentIndex: Math.max(0, view.modeNames.indexOf(view.mode))
                        Repeater {
                            model: view.modeNames
                            delegate: TabButton {
                                required property string modelData
                                text: modelData + (modelData === store.defaultMode() ? "  ★" : "")
                                width: implicitWidth + 16
                                onClicked: view.mode = modelData
                            }
                        }
                    }
                    ToolButton { text: "+"; font.pixelSize: 20; ToolTip.visible: hovered; ToolTip.text: qsTr("Add mode (copies the current one)"); onClicked: namePrompt.ask(qsTr("New mode"), view.mode + " 2", (n) => { const e = store.addMode(n, view.mode); if (!e.length) view.mode = n.trim(); return e; }) }
                    ToolButton { text: qsTr("Rename"); onClicked: namePrompt.ask(qsTr("Rename mode"), view.mode, (n) => { const e = store.renameMode(view.mode, n); if (!e.length) view.mode = n.trim(); return e; }) }
                    ToolButton { text: "★"; ToolTip.visible: hovered; ToolTip.text: qsTr("Start streams in this mode"); onClicked: store.setDefaultMode(view.mode) }
                    ToolButton {
                        text: qsTr("Delete")
                        enabled: view.modeNames.length > 1
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Delete mode")
                        onClicked: confirm.ask(qsTr("Delete mode"), qsTr("Delete mode \"%1\" and its bindings?").arg(view.mode), () => { store.removeMode(view.mode); view.ensureMode(); })
                    }
                }

                RowLayout {
                    Layout.leftMargin: 28; Layout.rightMargin: 28; Layout.bottomMargin: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 16

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.horizontalStretchFactor: 13
                        Layout.preferredWidth: 300
                        radius: 14
                        color: st.panel
                        border.color: st.border
                        RsControllerDiagram {
                            id: diagram
                            objectName: "diagramForHarness"
                            anchors.fill: parent
                            anchors.margins: 16
                            anchors.bottomMargin: 34
                            view: "front"
                            assignments: { store.revision; return store.assignments(view.mode); }
                            live: store.live.mode === view.mode ? store.live : ({})
                            onOutputClicked: (id) => selected = (selected === id ? "" : id)
                        }
                        Label {
                            anchors { left: parent.left; bottom: parent.bottom; margins: 14 }
                            text: diagram.selected ? qsTr("Showing bindings for %1 · click it again to show all").arg(diagram.selected) : qsTr("Click a control or its label to filter bindings · lights up live while streaming or testing")
                            color: st.textFaint
                            font.pixelSize: st.fontSmall
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.horizontalStretchFactor: 7
                        Layout.preferredWidth: 200
                        Layout.minimumWidth: 340
                        radius: 14
                        color: st.panel
                        border.color: st.border
                        ListView {
                            id: bindList
                            anchors.fill: parent
                            anchors.margins: 8
                            clip: true
                            spacing: 4
                            ScrollBar.vertical: ScrollBar {}
                            model: {
                                store.revision;
                                const all = store.bindings(view.mode);
                                const sel = diagram.selected;
                                if (!sel)
                                    return all;
                                return all.filter(b => (b.action === "Button" && b.button === sel) || b.action === sel || (sel.endsWith("Stick") && b.action.startsWith(sel)));
                            }
                            header: Label {
                                width: bindList.width
                                padding: 8
                                text: qsTr("%1 bindings").arg(bindList.count)
                                color: st.textFaint
                                font.pixelSize: st.fontSmall
                            }
                            delegate: Rectangle {
                                required property var modelData
                                width: bindList.width - 12
                                height: 46
                                radius: 8
                                color: rowMouse.containsMouse ? st.panel2 : "transparent"
                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bindingEditor.openExisting(parent.modelData)
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 4
                                    spacing: 10
                                    Rectangle {
                                        Layout.preferredWidth: Math.min(150, chordLabel.implicitWidth + 16)
                                        height: 28
                                        radius: 6
                                        color: "#0f1014"
                                        border.color: st.border
                                        Label { id: chordLabel; anchors.centerIn: parent; width: Math.min(implicitWidth, 140); elide: Text.ElideRight; text: modelData.chord; color: st.text; font.family: "monospace"; font.pixelSize: 13 }
                                    }
                                    Label { text: "→"; color: st.textFaint }
                                    Label { text: modelData.actionText; color: st.text; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Label { visible: modelData.bindMode !== "hold"; text: modelData.bindMode; color: st.warn; font.pixelSize: st.fontSmall }
                                    ToolButton {
                                        text: "✕"
                                        onClicked: store.removeBinding(view.mode, parent.parent.modelData.index)
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Remove binding")
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ==================================================== 2: Mouse & movement
            ScrollView {
                id: tunePage
                contentWidth: availableWidth
                clip: true
                readonly property var mc: { store.revision; return store.mouseConfig(view.mode); }
                readonly property var lk: { store.revision; return store.keyStickConfig(view.mode, "left"); }
                readonly property var rk: { store.revision; return store.keyStickConfig(view.mode, "right"); }

                ColumnLayout {
                    width: tunePage.availableWidth
                    spacing: 16
                    Item { height: 8 }
                    RowLayout {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        ColumnLayout {
                            spacing: 2
                            Label { text: qsTr("Mouse & Movement"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                            Label { text: qsTr("Settings apply per mode and take effect immediately, even mid-stream."); color: st.textDim }
                        }
                        Item { Layout.fillWidth: true }
                        Label { text: qsTr("Mode"); color: st.textDim }
                        ComboBox {
                            Layout.preferredWidth: 240
                            model: view.modeNames
                            currentIndex: view.modeNames.indexOf(view.mode)
                            onActivated: view.mode = currentText
                        }
                    }

                    GridLayout {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        Layout.fillWidth: true
                        columns: tunePage.availableWidth > 1150 ? 2 : 1
                        columnSpacing: 16
                        rowSpacing: 16

                        RsCard {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            title: qsTr("Mouse → analog stick")
                            subtitle: qsTr("Raw mouse counts are converted to stick deflection. Sensitivity 1.0 = full deflection at 2000 counts/s.")
                            RowLayout {
                                Label { text: qsTr("Mouse drives"); color: st.text; Layout.preferredWidth: 190 }
                                ComboBox {
                                    Layout.fillWidth: true
                                    readonly property var values: ["right_stick", "left_stick", "none"]
                                    model: [qsTr("Right stick (camera)"), qsTr("Left stick (movement / flight)"), qsTr("Nothing")]
                                    currentIndex: values.indexOf(tunePage.mc.target)
                                    onActivated: store.setMouseValue(view.mode, "target", values[currentIndex])
                                }
                            }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Sensitivity"); source: tunePage.mc.sensitivity || 0; from: 0.05; to: 5; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "sensitivity", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Horizontal"); hint: qsTr("Multiplier on X"); source: tunePage.mc.sens_x || 0; from: 0.1; to: 3; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "sens_x", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Vertical"); hint: qsTr("Multiplier on Y"); source: tunePage.mc.sens_y || 0; from: 0.1; to: 3; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "sens_y", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("ADS multiplier"); hint: qsTr("While aiming"); source: tunePage.mc.ads_multiplier || 0; from: 0.1; to: 2; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "ads_multiplier", v) }
                            RowLayout {
                                Switch { text: qsTr("ADS when L2 is held"); checked: !!tunePage.mc.ads_follows_l2; onToggled: store.setMouseValue(view.mode, "ads_follows_l2", checked) }
                                Switch { text: qsTr("Invert Y"); checked: !!tunePage.mc.invert_y; onToggled: store.setMouseValue(view.mode, "invert_y", checked) }
                            }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Smoothing"); hint: qsTr("0 = raw, higher = steadier but laggier"); source: tunePage.mc.smoothing || 0; from: 0; to: 1; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "smoothing", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Sample window"); hint: qsTr("Raise for 125 Hz mice"); source: tunePage.mc.sample_window_ms || 0; from: 2; to: 40; stepSize: 1; decimals: 0; suffix: " ms"; onCommitted: (v) => store.setMouseValue(view.mode, "sample_window_ms", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Acceleration"); hint: qsTr("0 = off (recommended)"); source: tunePage.mc.acceleration || 0; from: 0; to: 3; stepSize: 0.05; onCommitted: (v) => store.setMouseValue(view.mode, "acceleration", v) }
                            RsSliderRow { Layout.fillWidth: true; visible: (tunePage.mc.acceleration || 0) > 0; label: qsTr("Acceleration cap"); source: tunePage.mc.accel_cap || 0; from: 0; to: 4; stepSize: 0.05; onCommitted: (v) => store.setMouseValue(view.mode, "accel_cap", v) }
                        }

                        RsCard {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            title: qsTr("Response curve & deadzones")
                            subtitle: qsTr("Anti-deadzone pushes small movements past the game's own stick deadzone so slow aiming isn't swallowed.")
                            RsCurveChart {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 230
                                points: { store.revision; return store.curvePreview(view.mode, 120); }
                                customPoints: tunePage.mc.curve_points || []
                                editable: tunePage.mc.curve === "custom"
                                liveInput: store.live.mode === view.mode && store.live.mouseInput !== undefined && store.live.mouseInput > 0.001 ? store.live.mouseInput : -1
                                onCustomPointsEdited: (pts) => store.setMouseValue(view.mode, "curve_points", pts)
                            }
                            RowLayout {
                                Label { text: qsTr("Curve"); color: st.text; Layout.preferredWidth: 190 }
                                Repeater {
                                    model: [["linear", qsTr("Linear")], ["power", qsTr("Power")], ["custom", qsTr("Custom")]]
                                    delegate: RadioButton {
                                        required property var modelData
                                        text: modelData[1]
                                        checked: tunePage.mc.curve === modelData[0]
                                        onClicked: store.setMouseValue(view.mode, "curve", modelData[0])
                                    }
                                }
                            }
                            RsSliderRow { Layout.fillWidth: true; visible: tunePage.mc.curve === "power"; label: qsTr("Exponent"); hint: qsTr("< 1 boosts slow motion"); source: tunePage.mc.curve_exponent || 1; from: 0.3; to: 2.5; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "curve_exponent", v) }
                            Label { visible: tunePage.mc.curve === "custom"; text: qsTr("Drag the points on the chart to shape the curve."); color: st.textFaint; font.pixelSize: st.fontSmall }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Deadzone"); hint: qsTr("Ignore tiny jitter"); source: tunePage.mc.deadzone || 0; from: 0; to: 0.3; stepSize: 0.005; decimals: 3; onCommitted: (v) => store.setMouseValue(view.mode, "deadzone", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Anti-deadzone"); hint: qsTr("Match the game's inner deadzone"); source: tunePage.mc.anti_deadzone || 0; from: 0; to: 0.5; stepSize: 0.005; decimals: 3; onCommitted: (v) => store.setMouseValue(view.mode, "anti_deadzone", v) }
                            RowLayout {
                                Label { text: qsTr("Deadzone shape"); color: st.text; Layout.preferredWidth: 190 }
                                RadioButton { text: qsTr("Radial"); checked: tunePage.mc.anti_deadzone_shape !== "axial"; onClicked: store.setMouseValue(view.mode, "anti_deadzone_shape", "radial") }
                                RadioButton { text: qsTr("Axial"); checked: tunePage.mc.anti_deadzone_shape === "axial"; onClicked: store.setMouseValue(view.mode, "anti_deadzone_shape", "axial") }
                            }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Outer limit"); hint: qsTr("Maximum deflection"); source: tunePage.mc.outer_limit || 1; from: 0.3; to: 1; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "outer_limit", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Saturation compensation"); hint: qsTr("Let diagonals reach the stick corners"); source: tunePage.mc.square_saturation || 0; from: 0; to: 1; stepSize: 0.01; onCommitted: (v) => store.setMouseValue(view.mode, "square_saturation", v) }
                        }

                        RsCard {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            title: qsTr("Keyboard movement")
                            subtitle: qsTr("How keys bound to stick directions behave. Ramps give gradual analog steering instead of instant full lock.")
                            Label { text: qsTr("Left stick keys"); color: st.accent2; font.bold: true }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Ramp up"); hint: qsTr("Time to full deflection"); source: tunePage.lk.ramp_up_ms || 0; from: 0; to: 600; stepSize: 5; decimals: 0; suffix: " ms"; onCommitted: (v) => store.setKeyStickValue(view.mode, "left", "ramp_up_ms", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Return to centre"); source: tunePage.lk.ramp_down_ms || 0; from: 0; to: 600; stepSize: 5; decimals: 0; suffix: " ms"; onCommitted: (v) => store.setKeyStickValue(view.mode, "left", "ramp_down_ms", v) }
                            RowLayout {
                                Label { text: qsTr("Opposite keys"); color: st.text; Layout.preferredWidth: 190 }
                                ComboBox {
                                    Layout.fillWidth: true
                                    readonly property var values: ["last_wins", "neutral", "first_wins"]
                                    model: [qsTr("Last pressed wins"), qsTr("Cancel out (neutral)"), qsTr("First pressed wins")]
                                    currentIndex: values.indexOf(tunePage.lk.socd)
                                    onActivated: store.setKeyStickValue(view.mode, "left", "socd", values[currentIndex])
                                }
                            }
                            Switch { text: qsTr("Normalise diagonals (circular)"); checked: !!tunePage.lk.circular_diagonals; onToggled: store.setKeyStickValue(view.mode, "left", "circular_diagonals", checked) }
                            Label { text: qsTr("Right stick keys"); color: st.accent2; font.bold: true; Layout.topMargin: 8 }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Ramp up"); source: tunePage.rk.ramp_up_ms || 0; from: 0; to: 600; stepSize: 5; decimals: 0; suffix: " ms"; onCommitted: (v) => store.setKeyStickValue(view.mode, "right", "ramp_up_ms", v) }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Return to centre"); source: tunePage.rk.ramp_down_ms || 0; from: 0; to: 600; stepSize: 5; decimals: 0; suffix: " ms"; onCommitted: (v) => store.setKeyStickValue(view.mode, "right", "ramp_down_ms", v) }
                            Label { text: qsTr("Triggers"); color: st.accent2; font.bold: true; Layout.topMargin: 8 }
                            RsSliderRow { Layout.fillWidth: true; label: qsTr("Trigger ramp"); hint: qsTr("Gradual throttle / brake from keys"); source: { store.revision; return store.triggerRamp(view.mode); } from: 0; to: 600; stepSize: 5; decimals: 0; suffix: " ms"; onCommitted: (v) => store.setTriggerRamp(view.mode, v) }
                        }
                    }
                    Item { height: 20 }
                }
            }

            // ==================================================== 3: Live input
            ColumnLayout {
                spacing: 12
                Item { height: 8 }
                RowLayout {
                    Layout.leftMargin: 28; Layout.rightMargin: 28
                    ColumnLayout {
                        spacing: 2
                        Label { text: qsTr("Live Input"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                        Label {
                            text: store.sessionActive ? qsTr("Showing the controller state being sent to the console right now.")
                                                      : qsTr("Test your profile without a console: start the test pad, then use your keyboard and mouse.")
                            color: st.textDim
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Repeater {
                        model: view.modeNames
                        delegate: RsButton {
                            required property string modelData
                            text: modelData
                            highlighted: store.live.mode === modelData
                            enabled: store.sessionActive || store.previewActive
                            onClicked: store.switchSessionMode(modelData)
                        }
                    }
                }
                RowLayout {
                    Layout.leftMargin: 28; Layout.rightMargin: 28; Layout.bottomMargin: 20
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 16

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 14
                        color: st.panel
                        border.color: testPad.activeFocus ? st.accent : st.border
                        border.width: testPad.activeFocus ? 2 : 1
                        RsControllerDiagram {
                            anchors.fill: parent
                            anchors.margins: 16
                            anchors.bottomMargin: 60
                            view: "both"
                            assignments: { store.revision; return store.assignments(store.live.mode || view.mode); }
                            live: store.live
                        }
                        // Offline test pad: keyboard + relative mouse motion inside this panel.
                        FocusScope {
                            id: testPad
                            anchors.fill: parent
                            enabled: !store.sessionActive
                            Keys.onPressed: (event) => { if (store.previewActive && !event.isAutoRepeat) store.previewKey(event.key, event.nativeScanCode, event.modifiers, true); event.accepted = true; }
                            Keys.onReleased: (event) => { if (store.previewActive && !event.isAutoRepeat) store.previewKey(event.key, event.nativeScanCode, event.modifiers, false); event.accepted = true; }
                            Keys.onShortcutOverride: (event) => { event.accepted = store.previewActive; }
                            onActiveFocusChanged: if (!activeFocus) store.stopPreview()
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.AllButtons
                                property point last: Qt.point(-1, -1)
                                onPositionChanged: (mouse) => {
                                    if (store.previewActive && last.x >= 0)
                                        store.previewMouseDelta((mouse.x - last.x) * Screen.devicePixelRatio, (mouse.y - last.y) * Screen.devicePixelRatio);
                                    last = Qt.point(mouse.x, mouse.y);
                                }
                                onExited: last = Qt.point(-1, -1)
                                onPressed: (mouse) => {
                                    if (!store.previewActive) {
                                        store.startPreview();
                                        testPad.forceActiveFocus();
                                        return;
                                    }
                                    store.previewKeyName(store.keyFromMouseButton(mouse.button), true);
                                }
                                onReleased: (mouse) => store.previewKeyName(store.keyFromMouseButton(mouse.button), false)
                                onWheel: (wheel) => store.previewWheel(wheel.angleDelta.y)
                            }
                        }
                        Rectangle {
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10 }
                            height: 40
                            radius: 8
                            color: store.previewActive ? st.accentSoft : st.panel2
                            visible: !store.sessionActive
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 6
                                Label {
                                    Layout.fillWidth: true
                                    text: store.previewActive ? qsTr("Testing \"%1\" — keyboard and mouse movement over this panel drive the controller").arg(store.activeProfile)
                                                              : qsTr("Click the panel to start testing")
                                    color: store.previewActive ? st.text : st.textDim
                                    elide: Text.ElideRight
                                }
                                RsButton { visible: store.previewActive; text: qsTr("Stop"); flat: true; onClicked: { store.stopPreview(); view.forceActiveFocus(); } }
                            }
                        }
                    }

                    RsCard {
                        Layout.preferredWidth: 300
                        Layout.alignment: Qt.AlignTop
                        title: qsTr("Readout")
                        Readout { Layout.fillWidth: true; name: qsTr("Source"); value: store.live.source === "session" ? qsTr("Stream") : (store.live.source === "preview" ? qsTr("Test pad") : qsTr("Idle")) }
                        Readout { Layout.fillWidth: true; name: qsTr("Mode"); value: store.live.mode || "—" }
                        Readout { Layout.fillWidth: true; name: qsTr("Mouse captured"); value: store.live.captured ? qsTr("yes") : qsTr("no") }
                        Readout { Layout.fillWidth: true; name: qsTr("Raw Input"); value: store.live.source === "session" ? (store.live.rawInput ? qsTr("yes") : qsTr("no (cursor warp)")) : "—" }
                        Readout { Layout.fillWidth: true; name: qsTr("Update rate"); value: store.live.source === "session" ? qsTr("%1 Hz").arg(Math.round(store.live.tickHz || 0)) : "—" }
                        Readout { Layout.fillWidth: true; name: qsTr("Left stick"); value: (store.live.lx || 0).toFixed(2) + ", " + (store.live.ly || 0).toFixed(2) }
                        Readout { Layout.fillWidth: true; name: qsTr("Right stick"); value: (store.live.rx || 0).toFixed(2) + ", " + (store.live.ry || 0).toFixed(2) }
                        Readout { Layout.fillWidth: true; name: qsTr("L2 / R2"); value: Math.round((store.live.l2 || 0) * 100) + "% / " + Math.round((store.live.r2 || 0) * 100) + "%" }
                        Readout { Layout.fillWidth: true; name: qsTr("Mouse speed"); value: Math.round(Math.hypot(store.live.mouseVx || 0, store.live.mouseVy || 0)) + " c/s" }
                        Readout { Layout.fillWidth: true; name: qsTr("ADS"); value: store.live.ads ? qsTr("on (×%1)").arg((store.live.sensScale || 1).toFixed(2)) : qsTr("off") }
                        Readout { Layout.fillWidth: true; name: qsTr("Keys held"); value: String(store.live.heldKeys || 0) }
                    }
                }
            }

            // ==================================================== 4: Settings
            ScrollView {
                id: settingsPage
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: Math.min(settingsPage.availableWidth, 900)
                    spacing: 16
                    Item { height: 8 }
                    ColumnLayout {
                        Layout.leftMargin: 28
                        spacing: 2
                        Label { text: qsTr("Input Settings"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                        Label { text: qsTr("Applies to all profiles."); color: st.textDim }
                    }
                    RsCard {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        Layout.fillWidth: true
                        title: qsTr("Keyboard & mouse engine")
                        Switch {
                            text: qsTr("Use RemoteShift keyboard & mouse for streams")
                            checked: store.enabled
                            onToggled: store.enabled = checked
                        }
                        Label {
                            text: qsTr("When off, the original chiaki-ng keyboard mapping is used. Takes effect on the next stream.")
                            color: st.textFaint; font.pixelSize: st.fontSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        RowLayout {
                            Label { text: qsTr("Capture / release mouse"); color: st.text; Layout.preferredWidth: 240 }
                            ComboBox {
                                Layout.preferredWidth: 200
                                model: store.keyNames.filter(k => !k.startsWith("Mouse") && !k.startsWith("Wheel"))
                                currentIndex: model.indexOf(store.captureKey)
                                onActivated: store.captureKey = currentText
                            }
                        }
                        Label {
                            text: qsTr("Click the stream to capture the mouse. While captured, every key goes to the game (window shortcuts like Ctrl+O are paused) — press %1 to release, then Ctrl+O opens the stream menu.").arg(store.captureKey)
                            color: st.textFaint; font.pixelSize: st.fontSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        RowLayout {
                            Label { text: qsTr("Input update rate"); color: st.text; Layout.preferredWidth: 240 }
                            ComboBox {
                                Layout.preferredWidth: 200
                                readonly property var values: [125, 250, 500, 1000]
                                model: ["125 Hz", "250 Hz", "500 Hz", "1000 Hz"]
                                currentIndex: Math.max(0, values.indexOf(store.tickRate))
                                onActivated: store.tickRate = values[currentIndex]
                            }
                        }
                        Label {
                            text: qsTr("How often controller state is recomputed for mouse motion and ramps. Key presses are always sent immediately. 250 Hz is a good default; higher rates send more packets for little gain.")
                            color: st.textFaint; font.pixelSize: st.fontSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true
                        }
                        Switch {
                            text: qsTr("Show input mode on the stream")
                            checked: store.overlayEnabled
                            onToggled: store.overlayEnabled = checked
                        }
                    }
                    RsCard {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        Layout.fillWidth: true
                        title: qsTr("Updates")
                        subtitle: store.updateCheckAvailable ? qsTr("RemoteShift %1 · checks this project's GitHub Releases. Only the public release list is requested.").arg(store.appVersion)
                                                             : qsTr("RemoteShift %1 · update checks are disabled in this build (no release repository configured).").arg(store.appVersion)
                        Switch {
                            enabled: store.updateCheckAvailable
                            text: qsTr("Check for updates on startup")
                            checked: store.updateCheckEnabled && store.updateCheckAvailable
                            onToggled: store.updateCheckEnabled = checked
                        }
                        RowLayout {
                            RsButton { text: qsTr("Check now"); enabled: store.updateCheckAvailable; onClicked: store.checkForUpdates() }
                            RsButton { visible: store.updateUrl !== ""; text: qsTr("Open release page"); highlighted: true; onClicked: Qt.openUrlExternally(store.updateUrl) }
                            Label { text: store.updateStatus; color: st.textDim; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        }
                    }
                    RsCard {
                        Layout.leftMargin: 28; Layout.rightMargin: 28
                        Layout.fillWidth: true
                        title: qsTr("How it works")
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: st.textDim
                            text: qsTr("RemoteShift turns your keyboard and mouse into DualSense input inside the Remote Play connection itself — no virtual controller driver, no admin rights. On Windows, mouse motion is read with Raw Input (unaccelerated, at your mouse's full polling rate) and the cursor is confined to the stream while captured. A connected physical controller keeps working alongside the keyboard and mouse.")
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        visible: view.toastText.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: toastLabel.implicitWidth + 32
        height: 40
        radius: 20
        color: st.panel3
        border.color: st.border
        Label { id: toastLabel; anchors.centerIn: parent; text: view.toastText; color: st.text }
    }
}
