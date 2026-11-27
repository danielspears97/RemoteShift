import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.streetpea.chiaking

// Home: pick a console, pick a profile, connect.
ScrollView {
    id: page
    property var shell
    readonly property var store: Chiaki.remoteShift
    readonly property var diag: Chiaki.diagnostics
    readonly property var hosts: shell.hostList
    property string selectedKey: ""
    function keyOf(e) { return e.h.mac || e.h.duid || e.h.address || e.h.name; }
    readonly property var selected: {
        for (const e of hosts)
            if (keyOf(e) === selectedKey)
                return e;
        // Default: a registered console that is on, then any registered one, then the first.
        let best = null;
        for (const e of hosts) {
            const score = (e.h.registered ? 2 : 0) + (e.h.state === "ready" ? 1 : 0);
            if (!best || score > best.score)
                best = { e: e, score: score };
        }
        return best ? best.e : null;
    }
    contentWidth: availableWidth
    clip: true
    RsStyle { id: st }

    ColumnLayout {
        width: Math.min(page.availableWidth, 1760)
        spacing: 16
        Item { Layout.preferredHeight: 8 }

        RowLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            ColumnLayout {
                spacing: 2
                Label { text: qsTr("Home"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                Label {
                    text: page.hosts.length ? qsTr("%n console(s) available", "", page.hosts.length)
                                            : (Chiaki.discoveryEnabled ? qsTr("Looking for consoles on your network…") : qsTr("Network discovery is off"))
                    color: st.textDim
                }
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                visible: page.store.updateVersion !== ""
                radius: 19; implicitHeight: 38; implicitWidth: updRow.implicitWidth + 20
                color: st.accentSoft; border.color: st.accent
                RowLayout {
                    id: updRow
                    anchors.centerIn: parent
                    Label { text: qsTr("Update available: %1").arg(page.store.updateVersion); color: st.text }
                    RsButton { text: qsTr("Download"); flat: true; onClicked: Qt.openUrlExternally(page.store.updateUrl) }
                }
            }
        }

        RowLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            spacing: 16

            // ---------------- console hero
            Rectangle {
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 3
                Layout.preferredWidth: 300
                Layout.preferredHeight: 300
                radius: 16
                border.color: st.border
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#221f45" }
                    GradientStop { position: 0.7; color: st.panel }
                }

                // Empty state
                ColumnLayout {
                    visible: !page.selected
                    anchors { fill: parent; margins: 28 }
                    spacing: 8
                    Label { text: qsTr("No console yet"); font.pixelSize: 24; font.bold: true; color: st.text }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: st.textDim
                        lineHeight: 1.35
                        text: qsTr("1.  On the PS5 open Settings → System → Remote Play and turn on Enable Remote Play.\n2.  Keep the PS5 on (or in rest mode with “Stay Connected to the Internet” enabled) and on the same network as this PC.\n3.  It appears here within a few seconds. If it doesn't, add it by IP address.")
                    }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        spacing: 10
                        RsButton { text: qsTr("Add console by IP"); highlighted: true; onClicked: if (page.shell.app) page.shell.app.showManualHostDialog() }
                        RsButton { visible: !Chiaki.discoveryEnabled; text: qsTr("Turn discovery on"); onClicked: Chiaki.discoveryEnabled = true }
                    }
                }

                // Selected console
                RowLayout {
                    visible: !!page.selected
                    anchors { fill: parent; margins: 28 }
                    spacing: 28
                    Image {
                        Layout.preferredWidth: 230
                        Layout.preferredHeight: 200
                        Layout.alignment: Qt.AlignVCenter
                        fillMode: Image.PreserveAspectFit
                        sourceSize: Qt.size(460, 400)
                        source: page.selected ? "image://svg/console-ps" + (page.selected.h.ps5 === false ? "4" : "5") + (page.selected.h.state === "standby" ? "#light_standby" : "#light_on") : ""
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6
                        RowLayout {
                            spacing: 8
                            Rectangle {
                                readonly property var status: page.selected ? page.shell.hostStatus(page.selected.h) : ({ text: "", color: st.textFaint })
                                radius: 11; implicitHeight: 22; implicitWidth: chipRow.implicitWidth + 16
                                color: "#14151a"; border.color: st.border
                                RowLayout {
                                    id: chipRow
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Rectangle { width: 8; height: 8; radius: 4; color: parent.parent.status.color }
                                    Label { text: parent.parent.status.text; color: st.text; font.pixelSize: st.fontSmall }
                                }
                            }
                            Label {
                                visible: !!page.selected && !page.selected.h.registered && !(page.selected.h.duid && !page.selected.h.discovered)
                                text: qsTr("Not registered")
                                color: st.warn
                                font.pixelSize: st.fontSmall
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: page.selected ? (page.selected.h.name || qsTr("PlayStation")) : ""
                            font.pixelSize: 34; font.bold: true; color: st.text
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: st.textDim
                            text: {
                                if (!page.selected) return "";
                                const h = page.selected.h;
                                let parts = [h.ps5 === false ? "PS4" : "PS5"];
                                const a = page.shell.hostAddress(h);
                                if (a) parts.push(a);
                                if (h.app) parts.push(qsTr("Running: %1").arg(h.app));
                                return parts.join("  ·  ");
                            }
                        }
                        Item { Layout.fillHeight: true }
                        RowLayout {
                            spacing: 14
                            Label { text: qsTr("Play with"); color: st.textDim }
                            ComboBox {
                                Layout.preferredWidth: 260
                                model: page.store.profiles
                                currentIndex: page.store.profiles.indexOf(page.store.activeProfile)
                                onActivated: page.store.activeProfile = currentText
                            }
                            Label { text: page.shell.streamSummary(); color: st.textFaint }
                        }
                        RowLayout {
                            Layout.topMargin: 6
                            spacing: 10
                            RsButton {
                                id: connectButton
                                text: page.selected ? page.shell.connectLabel(page.selected.h) : ""
                                highlighted: true
                                font.pixelSize: 18
                                font.bold: true
                                leftPadding: 44; rightPadding: 44; topPadding: 16; bottomPadding: 16
                                onClicked: page.shell.connectHost(page.selected)
                            }
                            RsButton {
                                visible: !!page.selected && page.shell.canWake(page.selected.h)
                                text: qsTr("Wake")
                                topPadding: 16; bottomPadding: 16
                                onClicked: Chiaki.wakeUpHost(page.selected.idx)
                            }
                            RsButton { text: qsTr("Manage consoles"); flat: true; topPadding: 16; bottomPadding: 16; onClicked: page.shell.page = 6 }
                        }
                    }
                }
            }

            // ---------------- profile card
            RsCard {
                Layout.fillWidth: true
                Layout.horizontalStretchFactor: 2
                Layout.preferredWidth: 200
                Layout.preferredHeight: 300
                Layout.minimumWidth: 320
                id: profileCard
                title: qsTr("Input profile")
                subtitle: qsTr("Keyboard & mouse layout used for the next stream")
                readonly property var info: { page.store.revision; return page.store.profileInfo(page.store.activeProfile); }
                Label { text: page.store.activeProfile; font.pixelSize: 24; font.bold: true; color: st.text; elide: Text.ElideRight; Layout.fillWidth: true }
                Label {
                    text: qsTr("%1 modes · %2 bindings · release mouse with %3").arg(profileCard.info.modes || 0).arg(profileCard.info.bindings || 0).arg(page.store.captureKey)
                    color: st.textDim; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: { page.store.revision; return page.store.modes(); }
                        delegate: Rectangle {
                            required property string modelData
                            radius: 6; height: 26; width: modeLabel.implicitWidth + 16
                            color: "#14151a"; border.color: st.border
                            Label { id: modeLabel; anchors.centerIn: parent; text: modelData; color: st.accent2; font.pixelSize: st.fontSmall }
                        }
                    }
                }
                Label { visible: page.store.recentProfiles.length > 1; text: qsTr("Recently used"); color: st.textFaint; font.pixelSize: st.fontSmall; Layout.topMargin: 6 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: page.store.recentProfiles.length > 1
                    Repeater {
                        model: page.store.recentProfiles
                        delegate: RsButton {
                            required property string modelData
                            text: modelData
                            highlighted: modelData === page.store.activeProfile
                            onClicked: page.store.activeProfile = modelData
                        }
                    }
                }
                Item { Layout.fillHeight: true; Layout.preferredHeight: 4 }
                RowLayout {
                    spacing: 8
                    RsButton { text: qsTr("Edit keybinds"); onClicked: page.shell.page = 1 }
                    RsButton { text: qsTr("Test it"); onClicked: page.shell.page = 3 }
                    RsButton { text: qsTr("All profiles"); flat: true; onClicked: page.shell.page = 0 }
                }
            }
        }

        // ---------------- other consoles
        Label {
            visible: page.hosts.length > 1
            Layout.leftMargin: 28
            text: qsTr("Consoles")
            color: st.textDim
            font.bold: true
        }
        Flow {
            visible: page.hosts.length > 1
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            spacing: 12
            Repeater {
                model: page.hosts
                delegate: Rectangle {
                    required property var modelData
                    readonly property bool current: page.selected && page.keyOf(modelData) === page.keyOf(page.selected)
                    readonly property var status: page.shell.hostStatus(modelData.h)
                    width: 260; height: 64; radius: 12
                    color: current ? st.accentSoft : (hostMouse.containsMouse ? st.panel2 : st.panel)
                    border.color: current ? st.accent : st.border
                    RowLayout {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 14 }
                        spacing: 10
                        Rectangle { width: 10; height: 10; radius: 5; color: parent.parent.status.color }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label { text: modelData.h.name || qsTr("PlayStation"); color: st.text; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Label { text: parent.parent.parent.status.text + (modelData.h.registered ? "" : " · " + qsTr("not registered")); color: st.textDim; font.pixelSize: st.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                    }
                    MouseArea {
                        id: hostMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.selectedKey = page.keyOf(parent.modelData)
                        onDoubleClicked: page.shell.connectHost(parent.modelData)
                    }
                }
            }
        }

        // ---------------- status tiles
        RowLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            spacing: 16
            RsCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignTop
                title: qsTr("Stream quality")
                Label { text: page.shell.streamSummary(); color: st.text; font.pixelSize: 18; font.bold: true }
                Label { text: qsTr("Local PS5 connection. Remote Play tops out at 1080p60."); color: st.textDim; font.pixelSize: st.fontSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RsButton { text: qsTr("Change"); onClicked: page.shell.openSettings(1) }
            }
            RsCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignTop
                id: sessionCard
                title: page.diag.streaming ? qsTr("Current session") : qsTr("Last session")
                readonly property var tip: page.diag.tips.length ? page.diag.tips[0] : null
                Label {
                    text: !page.diag.hasData ? qsTr("No stream yet") : (sessionCard.tip ? sessionCard.tip.title : qsTr("Collecting data…"))
                    color: !page.diag.hasData ? st.textDim : (sessionCard.tip && sessionCard.tip.level === "good" ? st.good : (sessionCard.tip && sessionCard.tip.level === "bad" ? st.bad : st.warn))
                    font.pixelSize: 18; font.bold: true; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                Label {
                    text: page.diag.hasData ? qsTr("%1 fps · %2 Mbps · %3% loss").arg(Number(page.diag.current.fps || 0).toFixed(0)).arg(Number(page.diag.current.bitrate || 0).toFixed(1)).arg(Number(page.diag.current.loss || 0).toFixed(1))
                                            : qsTr("Frame rate, bitrate, packet loss and advice appear here after you connect.")
                    color: st.textDim; font.pixelSize: st.fontSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                RsButton { text: qsTr("Diagnostics"); onClicked: page.shell.page = 7 }
            }
            RsCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Layout.alignment: Qt.AlignTop
                title: qsTr("While streaming")
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: st.textDim
                    lineHeight: 1.3
                    text: qsTr("Click the stream to capture the mouse.\n%1 releases it.  Ctrl+O opens the stream menu.\nF11 toggles fullscreen.").arg(page.store.captureKey)
                }
                RsButton { text: qsTr("Input settings"); onClicked: page.shell.page = 4 }
            }
        }
        Item { Layout.preferredHeight: 20 }
    }
}
