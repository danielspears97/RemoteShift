import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import org.streetpea.chiaking

// Diagnostics: live values, five-minute history and advice. Everything shown is measured by the
// app; values it cannot measure are labelled as unavailable instead of being estimated.
ScrollView {
    id: page
    property var shell
    readonly property var diag: Chiaki.diagnostics
    readonly property var cur: diag.current
    readonly property var sess: diag.session
    contentWidth: availableWidth
    clip: true
    RsStyle { id: st }

    function fmt(v, d, unit) { return v === undefined || v === null || v < 0 ? "—" : Number(v).toFixed(d) + (unit || ""); }
    function series(name) { diag.revision; return diag.series(name); }

    FileDialog {
        id: exportDialog
        title: qsTr("Export diagnostics report")
        nameFilters: [qsTr("Text files (*.txt)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        onAccepted: {
            const err = page.diag.exportReport(selectedFile);
            page.shell.toast(err.length ? err : qsTr("Report saved"));
        }
    }

    component Tile: Rectangle {
        id: tile
        property string name
        property string value
        property string sub
        property color valueColor: st.text
        Layout.fillWidth: true
        Layout.preferredWidth: 100
        Layout.preferredHeight: 86
        radius: st.radius; color: st.panel; border.color: st.border
        ColumnLayout {
            anchors { fill: parent; margins: 12 }
            spacing: 0
            Label { text: tile.name; color: st.textDim; font.pixelSize: st.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
            Label { text: tile.value; color: tile.valueColor; font.pixelSize: 24; font.bold: true }
            Label { text: tile.sub; color: st.textFaint; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }

    ColumnLayout {
        width: Math.min(page.availableWidth, 1700)
        spacing: 14
        Item { Layout.preferredHeight: 8 }

        RowLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            ColumnLayout {
                spacing: 2
                Label { text: qsTr("Diagnostics"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                Label {
                    color: page.diag.streaming ? st.good : st.textDim
                    text: {
                        if (!page.diag.hasData && !page.diag.streaming)
                            return qsTr("Nothing recorded yet. Connect to a console and this page fills in live.");
                        const s = page.sess;
                        const what = "%1×%2 · %3 fps · %4 · %5".arg(s.width).arg(s.height).arg(s.targetFps).arg(s.codec).arg(qsTr("decoder: %1").arg(s.decoder));
                        const mins = Math.floor(page.diag.seconds / 60), secs = page.diag.seconds % 60;
                        return (page.diag.streaming ? qsTr("● Streaming") : qsTr("Last session")) + "  ·  " + what + "  ·  " + qsTr("%1:%2 recorded").arg(mins).arg(String(secs).padStart(2, "0"));
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Switch {
                text: qsTr("Show stats on the stream")
                checked: Chiaki.settings.showStreamStats
                onToggled: Chiaki.settings.showStreamStats = checked
            }
            RsButton { text: qsTr("Export report…"); enabled: page.diag.hasData; onClicked: { exportDialog.currentFile = "file:///RemoteShift-diagnostics.txt"; exportDialog.open(); } }
        }

        // ---- live values
        GridLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            columns: page.availableWidth > 1250 ? 8 : 4
            columnSpacing: 12; rowSpacing: 12
            Tile { name: qsTr("Source frame rate"); value: page.fmt(page.cur.fps, 0); sub: page.sess.targetFps ? qsTr("of %1 fps requested").arg(page.sess.targetFps) : qsTr("decoded frames per second") }
            Tile { name: qsTr("Bitrate"); value: page.fmt(page.cur.bitrate, 1); sub: qsTr("Mbps received") }
            Tile { name: qsTr("Packet loss"); value: page.fmt(page.cur.loss, 1, "%"); valueColor: (page.cur.loss || 0) >= 1 ? st.bad : ((page.cur.loss || 0) >= 0.2 ? st.warn : st.text); sub: qsTr("%1 frames lost in total").arg(page.cur.lostTotal || 0) }
            Tile { name: qsTr("Network round trip"); value: page.fmt(page.cur.rtt, 1); sub: (page.cur.rtt || -1) > 0 ? qsTr("ms, measured when connecting") : qsTr("not measured for this connection") }
            Tile { name: qsTr("Frame wait"); value: page.fmt(page.cur.frameAge, 0); sub: qsTr("ms before display · queue %1").arg(page.fmt(page.cur.queue, 1)) }
            Tile { name: qsTr("Dropped by renderer"); value: page.cur.droppedTotal === undefined ? "—" : String(page.cur.droppedTotal); sub: qsTr("frames this session") }
            Tile { name: qsTr("App CPU"); value: page.fmt(page.cur.cpu, 0, "%"); sub: qsTr("RemoteShift, share of all cores") }
            Tile { name: qsTr("Input update rate"); value: page.fmt(page.cur.inputHz, 0); sub: (page.cur.inputHz || -1) > 0 ? qsTr("Hz · %1").arg(page.cur.inputMode || "") : qsTr("keyboard & mouse engine off") }
        }
        Label {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: st.textFaint
            font.pixelSize: st.fontSmall
            text: qsTr("Not available: GPU utilisation, per-frame decode and render time, and live network latency. Remote Play only measures the round trip once while connecting, and RemoteShift does not show numbers it cannot measure.")
        }

        // ---- advice
        RsCard {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            title: qsTr("What to fix")
            subtitle: qsTr("Based on the last 30 seconds of the session")
            Label { visible: page.diag.tips.length === 0; text: page.diag.streaming ? qsTr("Collecting a few seconds of data…") : qsTr("Advice appears once a stream has been running for a few seconds."); color: st.textDim }
            Repeater {
                model: page.diag.tips
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 5
                        width: 10; height: 10; radius: 5
                        color: modelData.level === "good" ? st.good : (modelData.level === "bad" ? st.bad : (modelData.level === "info" ? st.accent2 : st.warn))
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Label { text: modelData.title; color: st.text; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Label { text: modelData.text; color: st.textDim; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
            }
        }

        // ---- history
        Label { Layout.leftMargin: 28; text: qsTr("History · last 5 minutes"); color: st.textDim; font.bold: true }
        GridLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            columns: page.availableWidth > 1250 ? 3 : 2
            columnSpacing: 12; rowSpacing: 12
            RsLineChart { Layout.fillWidth: true; Layout.preferredWidth: 100; title: qsTr("Source frame rate"); unit: "fps"; values: page.series("fps"); floorMax: 60; target: page.sess.targetFps || -1; lineColor: st.good }
            RsLineChart { Layout.fillWidth: true; Layout.preferredWidth: 100; title: qsTr("Bitrate"); unit: "Mbps"; decimals: 1; values: page.series("bitrate"); floorMax: 5; lineColor: st.accent }
            RsLineChart { Layout.fillWidth: true; Layout.preferredWidth: 100; title: qsTr("Packet loss"); unit: "%"; decimals: 1; values: page.series("loss"); floorMax: 2; lineColor: st.bad }
            RsLineChart { Layout.fillWidth: true; Layout.preferredWidth: 100; title: qsTr("Frame wait"); unit: "ms"; values: page.series("frameAge"); floorMax: 30; lineColor: st.accent2 }
            RsLineChart { Layout.fillWidth: true; Layout.preferredWidth: 100; title: qsTr("Frames lost + dropped"); note: qsTr("per second"); values: { const a = page.series("lost"), b = page.series("dropped"); return a.map((v, i) => v + (b[i] || 0)); } floorMax: 3; lineColor: st.warn }
            RsLineChart { Layout.fillWidth: true; Layout.preferredWidth: 100; title: qsTr("App CPU"); unit: "%"; values: page.series("cpu"); floorMax: 25; lineColor: "#90caf9" }
        }
        Item { Layout.preferredHeight: 20 }
    }
}
