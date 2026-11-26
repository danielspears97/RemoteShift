import QtQuick
import QtQuick.Shapes
import QtQuick.Controls

// Interactive controller diagram: shows which keys drive each output and lights up live input.
Item {
    id: diagram
    property var assignments: ({})   // output id -> [chord strings]
    property var live: ({})          // RsProfileStore.live
    property string selected: ""
    signal outputClicked(string id)

    implicitWidth: 640
    implicitHeight: 410
    RsStyle { id: st }

    readonly property int liveButtons: live && live.buttons !== undefined ? live.buttons : 0
    readonly property var bits: ({
        "Cross": 1, "Circle": 2, "Square": 4, "Triangle": 8, "DpadLeft": 16, "DpadRight": 32,
        "DpadUp": 64, "DpadDown": 128, "L1": 256, "R1": 512, "L3": 1024, "R3": 2048,
        "Options": 4096, "Create": 8192, "Touchpad": 16384, "PS": 32768
    })
    function pressed(id) { return (liveButtons & (bits[id] || 0)) !== 0; }
    function keysFor(id) { return assignments && assignments[id] ? assignments[id] : []; }
    function badge(id) {
        const k = keysFor(id);
        if (!k.length)
            return "";
        return k.length > 1 ? k[0] + " +" + (k.length - 1) : k[0];
    }

    Item {
        id: canvas
        width: 640
        height: 410
        anchors.centerIn: parent
        scale: Math.min(diagram.width / width, diagram.height / height)

        // Original simplified gamepad silhouette.
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: st.border
                strokeWidth: 2
                fillGradient: LinearGradient {
                    x1: 0; y1: 60; x2: 0; y2: 400
                    GradientStop { position: 0; color: "#262a34" }
                    GradientStop { position: 1; color: "#1b1d24" }
                }
                startX: 175; startY: 82
                PathLine { x: 465; y: 82 }
                PathCubic { x: 585; y: 140; control1X: 530; control1Y: 82; control2X: 570; control2Y: 100 }
                PathCubic { x: 600; y: 360; control1X: 610; control1Y: 210; control2X: 640; control2Y: 330 }
                PathCubic { x: 495; y: 320; control1X: 565; control1Y: 395; control2X: 525; control2Y: 345 }
                PathCubic { x: 145; y: 320; control1X: 420; control1Y: 285; control2X: 220; control2Y: 285 }
                PathCubic { x: 40; y: 360; control1X: 115; control1Y: 345; control2X: 75; control2Y: 395 }
                PathCubic { x: 55; y: 140; control1X: 0; control1Y: 330; control2X: 30; control2Y: 210 }
                PathCubic { x: 175; y: 82; control1X: 70; control1Y: 100; control2X: 110; control2Y: 82 }
            }
        }

        component PadButton: Item {
            id: pb
            property string outId
            property string label
            property string icon: ""
            property real size: 34
            property bool round: true
            property string badgeSide: "below"
            width: size
            height: size
            readonly property bool isPressed: diagram.pressed(outId)
            readonly property bool isSelected: diagram.selected === outId
            readonly property bool hasKeys: diagram.keysFor(outId).length > 0
            Rectangle {
                anchors.fill: parent
                radius: pb.round ? width / 2 : 6
                color: pb.isPressed ? st.accent : (pb.isSelected ? st.accentSoft : "#2d313c")
                border.color: pb.isSelected ? st.accent : (pb.hasKeys ? "#4a5060" : "#363a46")
                border.width: pb.isSelected ? 2 : 1
                Behavior on color { ColorAnimation { duration: 60 } }
            }
            Image {
                visible: pb.icon.length > 0
                anchors.centerIn: parent
                width: parent.width * 0.55
                height: width
                sourceSize: Qt.size(64, 64)
                source: pb.icon
            }
            Text {
                visible: pb.icon.length === 0
                anchors.centerIn: parent
                text: pb.label
                color: st.text
                font.pixelSize: pb.size > 30 ? 12 : 10
                font.bold: true
            }
            Rectangle {
                visible: badgeText.text.length > 0
                x: pb.badgeSide === "left" ? -width - 4 : (pb.badgeSide === "right" ? parent.width + 4 : (parent.width - width) / 2)
                y: pb.badgeSide === "below" ? parent.height + 3 : (pb.badgeSide === "above" ? -height - 3 : (parent.height - height) / 2)
                width: badgeText.implicitWidth + 10
                height: 18
                radius: 9
                color: "#0f1014"
                border.color: st.border
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: diagram.badge(pb.outId)
                    color: st.accent2
                    font.pixelSize: 10
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: diagram.outputClicked(pb.outId)
                ToolTip.visible: containsMouse && diagram.keysFor(pb.outId).length > 0
                ToolTip.text: pb.outId + ": " + diagram.keysFor(pb.outId).join(", ")
                ToolTip.delay: 300
            }
        }

        component Trigger: Item {
            id: tr
            property string outId
            property real amount: 0
            width: 86
            height: 26
            readonly property bool isSelected: diagram.selected === outId
            Rectangle {
                anchors.fill: parent
                radius: 8
                color: "#2d313c"
                border.color: tr.isSelected ? st.accent : st.border
                border.width: tr.isSelected ? 2 : 1
                clip: true
                Rectangle {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: parent.width * Math.max(0, Math.min(1, tr.amount))
                    radius: 8
                    color: st.accent
                }
                Text {
                    anchors.centerIn: parent
                    text: tr.outId
                    color: st.text
                    font.pixelSize: 12
                    font.bold: true
                }
            }
            Text {
                x: tr.outId === "L2" ? -implicitWidth - 6 : parent.width + 6
                anchors.verticalCenter: parent.verticalCenter
                text: diagram.badge(tr.outId)
                color: st.accent2
                font.pixelSize: 10
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: diagram.outputClicked(tr.outId)
                ToolTip.visible: containsMouse && diagram.keysFor(tr.outId).length > 0
                ToolTip.text: tr.outId + ": " + diagram.keysFor(tr.outId).join(", ")
                ToolTip.delay: 300
            }
        }

        component Stick: Item {
            id: sk
            property string prefix   // "LeftStick" / "RightStick"
            property string pressId  // "L3" / "R3"
            property real sx: 0
            property real sy: 0
            width: 74
            height: 74
            readonly property bool mouse: diagram.keysFor(prefix + "Mouse").length > 0
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "#171920"
                border.color: diagram.pressed(sk.pressId) ? st.accent : st.border
                border.width: 2
            }
            Rectangle {
                width: 30; height: 30; radius: 15
                x: parent.width / 2 - width / 2 + sk.sx * (parent.width / 2 - 15)
                y: parent.height / 2 - height / 2 + sk.sy * (parent.height / 2 - 15)
                color: (Math.abs(sk.sx) > 0.02 || Math.abs(sk.sy) > 0.02) ? st.accent : "#3a3f4c"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                anchors.topMargin: 4
                text: (sk.mouse ? qsTr("Mouse") : "") + (sk.mouse && keysText.length ? " · " : "") + keysText
                readonly property string keysText: {
                    const dirs = ["Up", "Left", "Down", "Right"];
                    let k = [];
                    for (const d of dirs) {
                        const a = diagram.keysFor(sk.prefix + d);
                        if (a.length)
                            k.push(a[0]);
                    }
                    return k.join("");
                }
                color: st.accent2
                font.pixelSize: 10
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: diagram.outputClicked(sk.prefix)
            }
        }

        Trigger { outId: "L2"; x: 110; y: 28; amount: diagram.live && diagram.live.l2 ? diagram.live.l2 : 0 }
        Trigger { outId: "R2"; x: 444; y: 28; amount: diagram.live && diagram.live.r2 ? diagram.live.r2 : 0 }
        PadButton { outId: "L1"; label: "L1"; x: 118; y: 60; size: 26; width: 70; round: false; badgeSide: "left" }
        PadButton { outId: "R1"; label: "R1"; x: 452; y: 60; size: 26; width: 70; round: false; badgeSide: "right" }

        PadButton { outId: "DpadUp"; label: "▲"; x: 133; y: 120; size: 30; round: false }
        PadButton { outId: "DpadDown"; label: "▼"; x: 133; y: 192; size: 30; round: false }
        PadButton { outId: "DpadLeft"; label: "◀"; x: 96; y: 156; size: 30; round: false }
        PadButton { outId: "DpadRight"; label: "▶"; x: 170; y: 156; size: 30; round: false }

        PadButton { outId: "Triangle"; icon: "image://svg/button-ps#pyramid"; x: 473; y: 112 }
        PadButton { outId: "Cross"; icon: "image://svg/button-ps#cross"; x: 473; y: 192 }
        PadButton { outId: "Square"; icon: "image://svg/button-ps#box"; x: 432; y: 152 }
        PadButton { outId: "Circle"; icon: "image://svg/button-ps#moon"; x: 514; y: 152 }

        PadButton { outId: "Touchpad"; label: qsTr("Touchpad"); x: 252; y: 92; size: 64; width: 136; round: false }
        PadButton { outId: "Create"; label: qsTr("Create"); x: 200; y: 96; size: 22; width: 44; round: false }
        PadButton { outId: "Options"; label: qsTr("Options"); x: 396; y: 96; size: 22; width: 52; round: false }
        PadButton { outId: "PS"; label: "PS"; x: 305; y: 196; size: 30 }

        Stick { prefix: "LeftStick"; pressId: "L3"; x: 196; y: 216; sx: diagram.live && diagram.live.lx ? diagram.live.lx : 0; sy: diagram.live && diagram.live.ly ? diagram.live.ly : 0 }
        Stick { prefix: "RightStick"; pressId: "R3"; x: 370; y: 216; sx: diagram.live && diagram.live.rx ? diagram.live.rx : 0; sy: diagram.live && diagram.live.ry ? diagram.live.ry : 0 }
        PadButton { outId: "L3"; label: "L3"; x: 160; y: 280; size: 24 }
        PadButton { outId: "R3"; label: "R3"; x: 456; y: 280; size: 24 }
    }
}
