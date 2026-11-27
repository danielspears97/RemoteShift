import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.streetpea.chiaking

// Consoles: every console RemoteShift knows about, with registration and management actions.
ScrollView {
    id: page
    property var shell
    readonly property var hosts: shell.hostList
    contentWidth: availableWidth
    clip: true
    RsStyle { id: st }

    ColumnLayout {
        width: Math.min(page.availableWidth, 1760)
        spacing: 14
        Item { Layout.preferredHeight: 8 }

        RowLayout {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            ColumnLayout {
                spacing: 2
                Label { text: qsTr("Consoles"); font.pixelSize: st.fontHuge; font.bold: true; color: st.text }
                Label { text: qsTr("Consoles found on your network, added by address, or linked through PSN."); color: st.textDim }
            }
            Item { Layout.fillWidth: true }
            Switch {
                text: qsTr("Network discovery")
                checked: Chiaki.discoveryEnabled
                onToggled: Chiaki.discoveryEnabled = checked
            }
            RsButton {
                text: Chiaki.settings.psnAuthToken ? qsTr("Refresh PSN consoles") : qsTr("Connect PSN")
                onClicked: {
                    if (Chiaki.settings.psnAuthToken) Chiaki.refreshPsnToken();
                    else if (page.shell.app) page.shell.app.showPSNTokenDialog(false);
                }
            }
            RsButton { text: qsTr("Add by IP"); highlighted: true; onClicked: if (page.shell.app) page.shell.app.showManualHostDialog() }
        }

        Rectangle {
            visible: page.hosts.length === 0
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            radius: st.radius; color: st.panel; border.color: st.border
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 6
                Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("No consoles found"); color: st.text; font.pixelSize: 18; font.bold: true }
                Label { Layout.alignment: Qt.AlignHCenter; text: qsTr("Enable Remote Play on the PS5 (Settings → System → Remote Play) and keep it on the same network, or add it by IP."); color: st.textDim }
            }
        }

        Repeater {
            model: page.hosts
            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property var h: modelData.h
                readonly property var status: page.shell.hostStatus(h)
                Layout.leftMargin: 28; Layout.rightMargin: 28
                Layout.fillWidth: true
                Layout.preferredHeight: 104
                radius: 12; color: st.panel; border.color: st.border
                RowLayout {
                    anchors { fill: parent; leftMargin: 18; rightMargin: 14 }
                    spacing: 18
                    Image {
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 80
                        fillMode: Image.PreserveAspectFit
                        sourceSize: Qt.size(192, 160)
                        source: "image://svg/console-ps" + (row.h.ps5 === false ? "4" : "5") + (row.h.state === "standby" ? "#light_standby" : "#light_on")
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        RowLayout {
                            spacing: 10
                            Label { text: row.h.name || qsTr("PlayStation"); color: st.text; font.pixelSize: 19; font.bold: true }
                            Rectangle { width: 9; height: 9; radius: 4.5; color: row.status.color }
                            Label { text: row.status.text; color: st.textDim }
                        }
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: st.textDim
                            font.pixelSize: st.fontSmall
                            text: {
                                let parts = [row.h.ps5 === false ? "PS4" : "PS5"];
                                const a = page.shell.hostAddress(row.h);
                                if (a) parts.push(a);
                                parts.push(row.h.registered ? qsTr("registered") : (row.h.duid ? (row.h.discovered ? qsTr("automatic registration available") : qsTr("remote connection via PSN")) : qsTr("not registered")));
                                if (row.h.manual) parts.push(qsTr("added manually"));
                                else if (row.h.discovered) parts.push(qsTr("discovered"));
                                return parts.join("  ·  ");
                            }
                        }
                        Label {
                            visible: !!row.h.app
                            text: qsTr("Running: %1").arg(row.h.app || "")
                            color: st.accent2
                            font.pixelSize: st.fontSmall
                        }
                    }
                    RsButton { visible: page.shell.removeLabel(row.h) !== ""; text: page.shell.removeLabel(row.h); danger: true; onClicked: page.shell.removeHost(row.modelData) }
                    RsButton { visible: row.h.registered; text: qsTr("Console PIN"); flat: true; onClicked: if (page.shell.app) page.shell.app.showConsolePinDialog(row.modelData.idx) }
                    RsButton { visible: page.shell.canWake(row.h); text: qsTr("Wake"); onClicked: Chiaki.wakeUpHost(row.modelData.idx) }
                    RsButton { text: page.shell.connectLabel(row.h); highlighted: true; onClicked: page.shell.connectHost(row.modelData) }
                }
            }
        }

        Label {
            Layout.leftMargin: 28; Layout.rightMargin: 28
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: st.textFaint
            font.pixelSize: st.fontSmall
            text: qsTr("Registering needs the 8-digit code from the PS5: Settings → System → Remote Play → Pair Device. Registration keys are stored only on this PC. Hidden consoles can be restored in App Settings → Consoles.")
        }
        Item { Layout.preferredHeight: 20 }
    }
}
