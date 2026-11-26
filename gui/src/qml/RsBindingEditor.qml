import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

// Modal editor for one binding: key capture, modifiers, action and options.
Popup {
    id: ed
    property var store
    property string mode
    property int index: -1            // -1 = new binding
    property var modeNames: []
    property string error: ""

    // working copy
    property string key: ""
    property var modifiers: []
    property string action: "Button"
    property string button: "Cross"
    property real value: 1
    property string targetMode: ""
    property string bindMode: "hold"
    property real turboHz: 10
    property bool capturing: false

    signal saved()

    modal: true
    Overlay.modal: Rectangle { color: "#b0000000" }
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(620, parent ? parent.width - 40 : 620)
    padding: 22
    closePolicy: capturing ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
    RsStyle { id: st }
    font.pixelSize: st.fontBody

    background: Rectangle {
        radius: 14
        color: st.panel
        border.color: st.border
    }

    readonly property var actionLabels: ({
        "Button": qsTr("Controller button"), "L2": qsTr("L2 trigger"), "R2": qsTr("R2 trigger"),
        "LeftStickUp": qsTr("Left stick up"), "LeftStickDown": qsTr("Left stick down"),
        "LeftStickLeft": qsTr("Left stick left"), "LeftStickRight": qsTr("Left stick right"),
        "RightStickUp": qsTr("Right stick up"), "RightStickDown": qsTr("Right stick down"),
        "RightStickLeft": qsTr("Right stick left"), "RightStickRight": qsTr("Right stick right"),
        "SetMode": qsTr("Switch to mode"), "HoldMode": qsTr("Hold for mode"), "CycleMode": qsTr("Next mode"),
        "AdsHold": qsTr("ADS sensitivity (hold)"), "SensitivityScale": qsTr("Sensitivity multiplier (hold)"),
        "ToggleCapture": qsTr("Capture / release mouse")
    })
    readonly property bool needsValue: ["L2", "R2", "LeftStickUp", "LeftStickDown", "LeftStickLeft", "LeftStickRight",
        "RightStickUp", "RightStickDown", "RightStickLeft", "RightStickRight", "SensitivityScale"].indexOf(action) >= 0
    readonly property bool needsMode: action === "SetMode" || action === "HoldMode"
    readonly property var commonModifiers: ["LShift", "LCtrl", "LAlt", "RShift", "RCtrl", "RAlt"]

    function openNew(preset) {
        index = -1;
        key = "";
        modifiers = [];
        action = preset && preset.action ? preset.action : "Button";
        button = preset && preset.button ? preset.button : "Cross";
        value = 1;
        targetMode = modeNames.length ? modeNames[0] : "";
        bindMode = "hold";
        turboHz = 10;
        error = "";
        open();
        capturing = true;
        captureArea.forceActiveFocus();
    }
    function openExisting(b) {
        index = b.index;
        key = b.key;
        modifiers = b.modifiers.slice();
        action = b.action;
        button = b.button;
        value = b.value;
        targetMode = b.mode || (modeNames.length ? modeNames[0] : "");
        bindMode = b.bindMode;
        turboHz = b.turboHz;
        error = "";
        capturing = false;
        open();
    }
    function toMap() {
        return { key: key, modifiers: modifiers, action: action, button: button, value: value,
                 mode: targetMode, bindMode: bindMode, turboHz: turboHz };
    }
    function save() {
        const err = index < 0 ? store.addBinding(mode, toMap()) : store.setBinding(mode, index, toMap());
        if (err.length) {
            error = err;
            return;
        }
        saved();
        close();
    }
    function setKey(k) {
        if (!k || k === "None")
            return;
        key = k;
        // A modifier pressed as the main key can't also be its own modifier.
        modifiers = modifiers.filter(m => m !== k);
        capturing = false;
    }
    function toggleModifier(m, on) {
        let mods = modifiers.filter(x => x !== m);
        if (on && m !== key)
            mods.push(m);
        modifiers = mods;
    }

    contentItem: ColumnLayout {
        spacing: 14

        Label {
            text: ed.index < 0 ? qsTr("Add binding") : qsTr("Edit binding")
            font.pixelSize: st.fontTitle
            font.bold: true
            color: st.text
        }

        // ---- key capture
        Rectangle {
            id: captureBox
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            radius: st.radius
            color: ed.capturing ? st.accentSoft : st.panel2
            border.color: ed.capturing ? st.accent : st.border
            border.width: ed.capturing ? 2 : 1

            FocusScope {
                id: captureArea
                anchors.fill: parent
                focus: ed.capturing
                Keys.onPressed: (event) => {
                    if (!ed.capturing)
                        return;
                    ed.setKey(ed.store.keyFromEvent(event.key, event.nativeScanCode, event.modifiers));
                    event.accepted = true;
                }
                Keys.onShortcutOverride: (event) => { if (ed.capturing) event.accepted = true; }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                onPressed: (mouse) => {
                    if (!ed.capturing) {
                        ed.capturing = true;
                        captureArea.forceActiveFocus();
                        return;
                    }
                    ed.setKey(ed.store.keyFromMouseButton(mouse.button));
                }
                onWheel: (wheel) => {
                    if (!ed.capturing)
                        return;
                    if (wheel.angleDelta.y !== 0)
                        ed.setKey(wheel.angleDelta.y > 0 ? "WheelUp" : "WheelDown");
                    else if (wheel.angleDelta.x !== 0)
                        ed.setKey(wheel.angleDelta.x > 0 ? "WheelRight" : "WheelLeft");
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 2
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: ed.capturing ? qsTr("Press a key, mouse button or scroll…")
                                       : (ed.key ? (ed.modifiers.length ? ed.modifiers.join(" + ") + " + " : "") + ed.key : qsTr("Click to choose a key"))
                    color: st.text
                    font.pixelSize: ed.capturing ? st.fontBody : 22
                    font.bold: !ed.capturing
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: ed.capturing ? qsTr("Every key is captured here, including Esc. Add modifiers below.") : qsTr("Click to capture again")
                    color: st.textFaint
                    font.pixelSize: st.fontSmall
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: qsTr("Or pick:"); color: st.textDim }
            ComboBox {
                id: keyPick
                Layout.fillWidth: true
                model: ed.store ? ed.store.keyNames : []
                currentIndex: ed.store ? ed.store.keyNames.indexOf(ed.key) : -1
                onActivated: ed.setKey(currentText)
            }
        }

        // ---- modifiers
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label { text: qsTr("Hold together with (modifiers)"); color: st.textDim; font.pixelSize: st.fontSmall }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: ed.commonModifiers.concat(ed.modifiers.filter(m => ed.commonModifiers.indexOf(m) < 0))
                    delegate: RsButton {
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: ed.modifiers.indexOf(modelData) >= 0
                        enabled: modelData !== ed.key
                        flat: !checked
                        highlighted: checked
                        Material.accent: st.accent
                        Material.roundedScale: Material.SmallScale
                        onToggled: ed.toggleModifier(modelData, checked)
                    }
                }
                ComboBox {
                    width: 150
                    displayText: qsTr("Other…")
                    model: ed.store ? ed.store.keyNames : []
                    onActivated: ed.toggleModifier(currentText, true)
                }
            }
        }

        // ---- action
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: qsTr("Action"); color: st.textDim }
            ComboBox {
                id: actionBox
                Layout.fillWidth: true
                model: ed.store ? ed.store.actionTypes : []
                displayText: ed.actionLabels[ed.action] || ed.action
                currentIndex: ed.store ? ed.store.actionTypes.indexOf(ed.action) : -1
                delegate: ItemDelegate {
                    required property string modelData
                    required property int index
                    width: ListView.view ? ListView.view.width : 200
                    text: ed.actionLabels[modelData] || modelData
                    highlighted: actionBox.highlightedIndex === index
                }
                onActivated: ed.action = currentText
            }

            Label { text: qsTr("Button"); color: st.textDim; visible: ed.action === "Button" }
            ComboBox {
                Layout.fillWidth: true
                visible: ed.action === "Button"
                model: ed.store ? ed.store.buttonNames : []
                currentIndex: ed.store ? ed.store.buttonNames.indexOf(ed.button) : -1
                onActivated: ed.button = currentText
            }

            Label { text: qsTr("Mode"); color: st.textDim; visible: ed.needsMode }
            ComboBox {
                Layout.fillWidth: true
                visible: ed.needsMode
                model: ed.modeNames
                currentIndex: ed.modeNames.indexOf(ed.targetMode)
                onActivated: ed.targetMode = currentText
            }

            Label { text: ed.action === "SensitivityScale" ? qsTr("Multiplier") : qsTr("Amount"); color: st.textDim; visible: ed.needsValue }
            RowLayout {
                visible: ed.needsValue
                Layout.fillWidth: true
                Slider {
                    id: valueSlider
                    Layout.fillWidth: true
                    from: ed.action === "SensitivityScale" ? 0.1 : 0.05
                    to: ed.action === "SensitivityScale" ? 3 : 1
                    stepSize: 0.05
                    value: ed.value
                    onMoved: ed.value = value
                    Material.accent: st.accent
                }
                Label {
                    text: ed.action === "SensitivityScale" ? "×" + ed.value.toFixed(2) : Math.round(ed.value * 100) + "%"
                    color: st.textDim
                    Layout.preferredWidth: 52
                }
            }

            Label { text: qsTr("Behaviour"); color: st.textDim }
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: [["hold", qsTr("Hold")], ["toggle", qsTr("Toggle")], ["turbo", qsTr("Turbo")]]
                    delegate: RadioButton {
                        required property var modelData
                        text: modelData[1]
                        checked: ed.bindMode === modelData[0]
                        onClicked: ed.bindMode = modelData[0]
                        Material.accent: st.accent
                    }
                }
                SpinBox {
                    visible: ed.bindMode === "turbo"
                    from: 1; to: 30
                    value: ed.turboHz
                    onValueModified: ed.turboHz = value
                    textFromValue: (v) => v + " Hz"
                    valueFromText: (t) => parseInt(t)
                }
            }
        }

        Label {
            visible: ed.error.length > 0
            text: ed.error
            color: st.bad
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            RsButton {
                visible: ed.index >= 0
                text: qsTr("Delete")
                flat: true
                danger: true
                onClicked: { ed.store.removeBinding(ed.mode, ed.index); ed.saved(); ed.close(); }
            }
            Item { Layout.fillWidth: true }
            RsButton {
                text: qsTr("Cancel")
                flat: true
                onClicked: ed.close()
            }
            RsButton {
                text: qsTr("Save")
                highlighted: true
                enabled: ed.key.length > 0 && !ed.capturing
                Material.accent: st.accent
                onClicked: ed.save()
            }
        }
    }
}
