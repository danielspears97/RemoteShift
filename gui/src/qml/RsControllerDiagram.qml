import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// Controller diagram with front and top (shoulder) views. Each control gets a callout with its
// bound keys and a leader line to the control; controls light up with live input.
Item {
    id: diagram
    property var assignments: ({})   // output id -> [chord strings]  (RsProfileStore.assignments)
    property var live: ({})          // RsProfileStore.live
    property string selected: ""
    property string view: "front"    // "front" | "top" | "both"
    property bool showToggle: true
    signal outputClicked(string id)

    RsStyle { id: st }

    // ---- geometry (design units; the stage scales uniformly to fit)
    readonly property real s: 540 / 600          // svg units -> design units
    readonly property int designW: 984
    readonly property int artX: 222
    readonly property int colW: 210
    readonly property int frontH: 392
    readonly property int topH: 220
    readonly property int viewGap: 0
    readonly property bool showFront: view !== "top"
    readonly property bool showTop: view !== "front"
    readonly property real designH: (showTop ? topH : 0) + (showTop && showFront ? viewGap : 0) + (showFront ? frontH : 0)

    readonly property var topIds: ["L1", "L2", "R1", "R2"]
    onSelectedChanged: {
        if (!selected || view === "both")
            return;
        const isTop = topIds.indexOf(selected) >= 0;
        if (isTop && view === "front")
            view = "top";
        else if (!isTop && view === "top")
            view = "front";
    }

    // Control anchors are in SVG units of the matching artwork.
    readonly property var views: [
        {
            key: "top", art: "qrc:/icons/controller-top.svg", artH: 230, artY: 6,
            controls: [
                { id: "L2", name: qsTr("L2 trigger"), side: "L", x: 155, y: 62, glow: { x: 100, y: 30, w: 110, h: 72, r: 18 }, trigger: true },
                { id: "L1", name: qsTr("L1"), side: "L", x: 150, y: 112, glow: { x: 84, y: 101, w: 140, h: 24, r: 10 } },
                { id: "R2", name: qsTr("R2 trigger"), side: "R", x: 445, y: 62, glow: { x: 390, y: 30, w: 110, h: 72, r: 18 }, trigger: true },
                { id: "R1", name: qsTr("R1"), side: "R", x: 450, y: 112, glow: { x: 376, y: 101, w: 140, h: 24, r: 10 } }
            ]
        },
        {
            key: "front", art: "qrc:/icons/controller-front.svg", artH: 400, artY: 4,
            controls: [
                { id: "DpadUp", name: qsTr("D-pad up"), side: "L", x: 150, y: 137, glow: { x: 137, y: 122, w: 26, h: 30, r: 7 } },
                { id: "DpadLeft", name: qsTr("D-pad left"), side: "L", x: 122, y: 165, glow: { x: 107, y: 152, w: 30, h: 26, r: 7 } },
                { id: "DpadRight", name: qsTr("D-pad right"), side: "L", x: 178, y: 165, glow: { x: 163, y: 152, w: 30, h: 26, r: 7 } },
                { id: "DpadDown", name: qsTr("D-pad down"), side: "L", x: 150, y: 193, glow: { x: 137, y: 178, w: 26, h: 30, r: 7 } },
                { id: "Create", name: qsTr("Create"), side: "L", x: 209, y: 104, glow: { x: 205, y: 96, w: 8, h: 22, r: 4 } },
                { id: "Touchpad", name: qsTr("Touchpad"), side: "L", x: 262, y: 100, glow: { x: 229, y: 77, w: 142, h: 92, r: 14 } },
                { id: "LeftStick", name: qsTr("Left stick"), side: "L", x: 188, y: 262, stick: "left" },
                { id: "L3", name: qsTr("L3 (press)"), side: "L", x: 222, y: 302, ring: { x: 222, y: 262 } },
                { id: "Options", name: qsTr("Options"), side: "R", x: 391, y: 104, glow: { x: 387, y: 96, w: 8, h: 22, r: 4 } },
                { id: "Triangle", name: qsTr("Triangle"), side: "R", x: 450, y: 123, glow: { x: 430.5, y: 103.5, w: 39, h: 39, r: 19.5 } },
                { id: "Circle", name: qsTr("Circle"), side: "R", x: 492, y: 165, glow: { x: 472.5, y: 145.5, w: 39, h: 39, r: 19.5 } },
                { id: "Square", name: qsTr("Square"), side: "R", x: 408, y: 165, glow: { x: 388.5, y: 145.5, w: 39, h: 39, r: 19.5 } },
                { id: "Cross", name: qsTr("Cross"), side: "R", x: 450, y: 207, glow: { x: 430.5, y: 187.5, w: 39, h: 39, r: 19.5 } },
                { id: "PS", name: qsTr("PS / Home"), side: "R", x: 300, y: 205, glow: { x: 288, y: 193, w: 24, h: 24, r: 12 } },
                { id: "RightStick", name: qsTr("Right stick"), side: "R", x: 412, y: 262, stick: "right" },
                { id: "R3", name: qsTr("R3 (press)"), side: "R", x: 378, y: 302, ring: { x: 378, y: 262 } }
            ]
        }
    ]

    // ---- state helpers
    readonly property int liveButtons: live && live.buttons !== undefined ? live.buttons : 0
    readonly property var bits: ({
        "Cross": 1, "Circle": 2, "Square": 4, "Triangle": 8, "DpadLeft": 16, "DpadRight": 32,
        "DpadUp": 64, "DpadDown": 128, "L1": 256, "R1": 512, "L3": 1024, "R3": 2048,
        "Options": 4096, "Create": 8192, "Touchpad": 16384, "PS": 32768
    })
    function num(v) { return v === undefined || v === null || isNaN(v) ? 0 : v; }
    function amount(id) {
        if (id === "L2") return num(live.l2);
        if (id === "R2") return num(live.r2);
        if (id === "LeftStick") return Math.min(1, Math.hypot(num(live.lx), num(live.ly)));
        if (id === "RightStick") return Math.min(1, Math.hypot(num(live.rx), num(live.ry)));
        return (liveButtons & (bits[id] || 0)) ? 1 : 0;
    }
    function isActive(id) { return amount(id) > 0.04; }
    function keysFor(id) { return assignments && assignments[id] ? assignments[id] : []; }
    function chipsFor(id) {
        if (id === "LeftStick" || id === "RightStick") {
            let chips = [];
            let dirs = "";
            let multi = false;
            for (const d of ["Up", "Left", "Down", "Right"]) {
                const k = keysFor(id + d);
                if (k.length) {
                    if (k[0].length === 1) dirs += k[0]; else multi = true;
                }
            }
            if (dirs.length)
                chips.push(dirs);
            else if (multi)
                chips.push(qsTr("Keys"));
            if (keysFor(id + "Mouse").length)
                chips.push(qsTr("Mouse"));
            return chips;
        }
        // One key per label keeps every callout readable; the rest are in the tooltip/list.
        const k = keysFor(id);
        if (k.length <= 1)
            return k;
        return [k[0], "+" + (k.length - 1)];
    }
    function tooltipFor(id) {
        if (id === "LeftStick" || id === "RightStick") {
            let parts = [];
            for (const d of ["Up", "Down", "Left", "Right"]) {
                const k = keysFor(id + d);
                if (k.length) parts.push(d.toLowerCase() + ": " + k.join(", "));
            }
            if (keysFor(id + "Mouse").length) parts.push(qsTr("mouse"));
            return parts.join(" · ");
        }
        return keysFor(id).join(", ");
    }

    // Spread callouts vertically without overlap, staying close to their anchors.
    function placeCallouts(controls, artY, height) {
        const rowH = 40, gap = 5;
        let out = [];
        for (const side of ["L", "R"]) {
            let items = controls.filter(c => c.side === side).map(c => ({ c: c, y: artY + c.y * s - rowH / 2 }));
            items.sort((a, b) => a.y - b.y);
            for (let i = 0; i < items.length; i++) {
                items[i].y = Math.max(items[i].y, 0);
                if (i > 0) items[i].y = Math.max(items[i].y, items[i - 1].y + rowH + gap);
            }
            for (let i = items.length - 1; i >= 0; i--) {
                const limit = i === items.length - 1 ? height - rowH : items[i + 1].y - rowH - gap;
                items[i].y = Math.min(items[i].y, limit);
            }
            out = out.concat(items);
        }
        return out;
    }

    // ---- view toggle
    Row {
        id: toggle
        visible: diagram.showToggle
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 0
        Repeater {
            model: [["front", qsTr("Front")], ["top", qsTr("Top · triggers")], ["both", qsTr("Both")]]
            delegate: Rectangle {
                required property var modelData
                required property int index
                readonly property bool active: diagram.view === modelData[0]
                width: segLabel.implicitWidth + 28
                height: 30
                radius: 8
                color: active ? st.accent : (segMouse.containsMouse ? st.panel3 : st.panel2)
                border.color: active ? st.accent : st.border
                Label {
                    id: segLabel
                    anchors.centerIn: parent
                    text: modelData[1]
                    color: parent.active ? "white" : st.textDim
                    font.pixelSize: 13
                    font.bold: parent.active
                }
                MouseArea {
                    id: segMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: diagram.view = parent.modelData[0]
                }
            }
        }
    }

    Item {
        id: area
        anchors.fill: parent
        anchors.topMargin: diagram.showToggle ? toggle.height + 12 : 0

        Item {
            id: stage
            width: diagram.designW
            height: diagram.designH
            transformOrigin: Item.TopLeft
            scale: Math.max(0.1, Math.min(1.2, area.width / width, area.height / height))
            x: (area.width - width * scale) / 2
            y: (area.height - height * scale) / 2

            Repeater {
                model: diagram.views
                delegate: Item {
                    id: vl
                    required property var modelData
                    readonly property bool isTop: modelData.key === "top"
                    visible: isTop ? diagram.showTop : diagram.showFront
                    width: diagram.designW
                    height: isTop ? diagram.topH : diagram.frontH
                    y: isTop || !diagram.showTop ? 0 : diagram.topH + diagram.viewGap
                    readonly property real artY: modelData.artY
                    readonly property var placed: diagram.placeCallouts(modelData.controls, artY, height)
                    function ax(v) { return diagram.artX + v * diagram.s; }
                    function ay(v) { return artY + v * diagram.s; }

                    Image {
                        x: diagram.artX
                        y: vl.artY
                        width: 600 * diagram.s
                        height: vl.modelData.artH * diagram.s
                        source: vl.modelData.art
                        sourceSize: Qt.size(Math.ceil(width * stage.scale * Screen.devicePixelRatio),
                                            Math.ceil(height * stage.scale * Screen.devicePixelRatio))
                        smooth: true
                        mipmap: true
                    }

                    // Live glow / selection outline on each control.
                    Repeater {
                        model: vl.modelData.controls
                        delegate: Item {
                            id: ctl
                            required property var modelData
                            readonly property real amt: diagram.amount(modelData.id)
                            readonly property bool sel: diagram.selected === modelData.id
                            readonly property bool hov: hit.containsMouse || vl.calloutHoverId === modelData.id

                            // Rect-shaped controls (buttons, touchpad, triggers)
                            Rectangle {
                                visible: !!ctl.modelData.glow
                                x: ctl.modelData.glow ? vl.ax(ctl.modelData.glow.x) : 0
                                y: ctl.modelData.glow ? vl.ay(ctl.modelData.glow.y) : 0
                                width: ctl.modelData.glow ? ctl.modelData.glow.w * diagram.s : 0
                                height: ctl.modelData.glow ? ctl.modelData.glow.h * diagram.s : 0
                                radius: ctl.modelData.glow ? ctl.modelData.glow.r * diagram.s : 0
                                color: ctl.modelData.trigger ? "transparent" : Qt.rgba(0.49, 0.42, 1, ctl.amt > 0.04 ? 0.6 : (ctl.hov ? 0.18 : 0))
                                border.color: ctl.sel ? st.accent : (ctl.amt > 0.04 ? "#b9b0ff" : "transparent")
                                border.width: ctl.sel ? 2.5 : 1.5
                                clip: true
                                Behavior on color { ColorAnimation { duration: 70 } }
                                // analog trigger fill
                                Rectangle {
                                    visible: !!ctl.modelData.trigger
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: parent.height * Math.min(1, ctl.amt)
                                    color: Qt.rgba(0.49, 0.42, 1, 0.6)
                                }
                                Rectangle {
                                    visible: !!ctl.modelData.trigger && ctl.hov && ctl.amt <= 0.04
                                    anchors.fill: parent
                                    color: Qt.rgba(0.49, 0.42, 1, 0.18)
                                }
                            }
                            // Stick press ring
                            Rectangle {
                                visible: !!ctl.modelData.ring
                                readonly property real r: 45 * diagram.s
                                x: ctl.modelData.ring ? vl.ax(ctl.modelData.ring.x) - r : 0
                                y: ctl.modelData.ring ? vl.ay(ctl.modelData.ring.y) - r : 0
                                width: r * 2
                                height: r * 2
                                radius: r
                                color: "transparent"
                                border.width: ctl.amt > 0.04 || ctl.sel ? 3 : (ctl.hov ? 2 : 0)
                                border.color: ctl.amt > 0.04 ? "#b9b0ff" : st.accent
                            }
                            // Stick cap that follows live deflection
                            Item {
                                visible: !!ctl.modelData.stick
                                readonly property bool isLeft: ctl.modelData.stick === "left"
                                readonly property real cx: vl.ax(isLeft ? 222 : 378)
                                readonly property real cy: vl.ay(262)
                                readonly property real dx: diagram.num(isLeft ? diagram.live.lx : diagram.live.rx)
                                readonly property real dy: diagram.num(isLeft ? diagram.live.ly : diagram.live.ry)
                                readonly property real travel: 11 * diagram.s
                                readonly property real r: 31 * diagram.s
                                Rectangle {
                                    x: parent.cx + parent.dx * parent.travel - parent.r
                                    y: parent.cy + parent.dy * parent.travel - parent.r
                                    width: parent.r * 2
                                    height: parent.r * 2
                                    radius: parent.r
                                    gradient: Gradient {
                                        GradientStop { position: 0; color: "#454a57" }
                                        GradientStop { position: 1; color: "#272a32" }
                                    }
                                    border.color: ctl.amt > 0.04 ? st.accent : (ctl.sel ? st.accent : (ctl.hov ? "#6b7080" : "#555a67"))
                                    border.width: ctl.amt > 0.04 || ctl.sel ? 2.5 : 1.5
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: parent.width * 0.66
                                        height: width
                                        radius: width / 2
                                        color: "#24262d"
                                        border.color: "#353843"
                                        border.width: 1
                                    }
                                }
                            }
                            MouseArea {
                                id: hit
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                x: ctl.modelData.glow ? vl.ax(ctl.modelData.glow.x) : vl.ax((ctl.modelData.ring || { x: ctl.modelData.stick === "left" ? 222 : 378 }).x) - 40 * diagram.s
                                y: ctl.modelData.glow ? vl.ay(ctl.modelData.glow.y) : vl.ay(ctl.modelData.ring ? 290 : 228)
                                width: ctl.modelData.glow ? ctl.modelData.glow.w * diagram.s : 80 * diagram.s
                                height: ctl.modelData.glow ? ctl.modelData.glow.h * diagram.s : (ctl.modelData.ring ? 24 : 60) * diagram.s
                                onClicked: diagram.outputClicked(ctl.modelData.id)
                                ToolTip.visible: containsMouse
                                ToolTip.delay: 250
                                ToolTip.text: ctl.modelData.name + (diagram.tooltipFor(ctl.modelData.id) ? ": " + diagram.tooltipFor(ctl.modelData.id) : qsTr(": unbound"))
                            }
                        }
                    }

                    property string calloutHoverId: ""

                    // Leader lines
                    Canvas {
                        id: leaders
                        anchors.fill: parent
                        readonly property var deps: [diagram.liveButtons, diagram.selected, vl.calloutHoverId, diagram.live.l2, diagram.live.r2,
                                                     diagram.live.lx, diagram.live.ly, diagram.live.rx, diagram.live.ry, vl.placed]
                        onDepsChanged: requestPaint()
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset();
                            for (const p of vl.placed) {
                                const c = p.c;
                                const active = diagram.isActive(c.id);
                                const hi = active || diagram.selected === c.id || vl.calloutHoverId === c.id;
                                const left = c.side === "L";
                                const sx = left ? diagram.colW : diagram.designW - diagram.colW;
                                const sy = p.y + 20;
                                const elbow = left ? diagram.artX - 4 : diagram.artX + 600 * diagram.s + 4;
                                const tx = vl.ax(c.x), ty = vl.ay(c.y);
                                ctx.strokeStyle = hi ? st.accent : "#3b404c";
                                ctx.lineWidth = hi ? 2 : 1.25;
                                ctx.beginPath();
                                ctx.moveTo(sx, sy);
                                ctx.lineTo(elbow, sy);
                                ctx.lineTo(tx, ty);
                                ctx.stroke();
                                ctx.fillStyle = hi ? st.accent : "#5a6070";
                                ctx.beginPath();
                                ctx.arc(tx, ty, hi ? 3.5 : 2.5, 0, Math.PI * 2);
                                ctx.fill();
                            }
                        }
                    }

                    // Callouts
                    Repeater {
                        model: vl.placed
                        delegate: Rectangle {
                            id: callout
                            required property var modelData
                            readonly property var c: modelData.c
                            readonly property bool isLeft: c.side === "L"
                            readonly property bool active: diagram.isActive(c.id)
                            readonly property bool sel: diagram.selected === c.id
                            // Right-column rows lay out right-to-left; reverse so chips still read "C +1".
                            readonly property var chips: isLeft ? diagram.chipsFor(c.id) : diagram.chipsFor(c.id).slice().reverse()
                            x: isLeft ? 0 : diagram.designW - diagram.colW
                            y: modelData.y
                            width: diagram.colW
                            height: 40
                            radius: 9
                            color: active ? st.accentSoft : (sel ? "#232046" : (cMouse.containsMouse ? st.panel3 : st.panel2))
                            border.color: active || sel ? st.accent : st.border
                            border.width: sel ? 2 : 1
                            Behavior on color { ColorAnimation { duration: 70 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 6
                                layoutDirection: callout.isLeft ? Qt.LeftToRight : Qt.RightToLeft
                                Label {
                                    text: callout.c.name
                                    color: callout.active ? st.text : st.textDim
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    horizontalAlignment: callout.isLeft ? Text.AlignLeft : Text.AlignRight
                                }
                                Repeater {
                                    model: callout.chips.length ? callout.chips : [""]
                                    delegate: Rectangle {
                                        required property string modelData
                                        readonly property bool empty: modelData === ""
                                        Layout.preferredHeight: 26
                                        Layout.preferredWidth: Math.min(104, chipText.implicitWidth + 14)
                                        radius: 6
                                        color: empty ? "transparent" : "#0f1014"
                                        border.color: empty ? "transparent" : (callout.active ? st.accent : "#3a3f4b")
                                        Label {
                                            id: chipText
                                            anchors.centerIn: parent
                                            width: Math.min(implicitWidth, 92)
                                            elide: Text.ElideRight
                                            text: parent.empty ? "—" : parent.modelData
                                            color: parent.empty ? st.textFaint : st.accent2
                                            font.pixelSize: 15
                                            font.family: "monospace"
                                            font.bold: !parent.empty
                                        }
                                    }
                                }
                            }
                            MouseArea {
                                id: cMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onContainsMouseChanged: vl.calloutHoverId = containsMouse ? callout.c.id : (vl.calloutHoverId === callout.c.id ? "" : vl.calloutHoverId)
                                onClicked: diagram.outputClicked(callout.c.id)
                                ToolTip.visible: containsMouse && diagram.tooltipFor(callout.c.id).length > 0
                                ToolTip.delay: 300
                                ToolTip.text: diagram.tooltipFor(callout.c.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
