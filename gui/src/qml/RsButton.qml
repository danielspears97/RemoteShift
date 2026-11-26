import QtQuick
import QtQuick.Controls

// RemoteShift button: primary (highlighted), secondary, ghost (flat) and danger variants.
Button {
    id: b
    property bool danger: false
    RsStyle { id: st }
    padding: 10
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: st.fontBody
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    contentItem: Text {
        text: b.text
        font: b.font
        color: b.highlighted ? "white" : (b.danger ? st.bad : st.text)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        implicitHeight: 38
        implicitWidth: 64
        radius: 8
        color: b.highlighted ? (b.down ? Qt.darker(st.accent, 1.15) : (b.hovered ? Qt.lighter(st.accent, 1.1) : st.accent))
             : b.flat || b.danger ? (b.hovered ? st.panel2 : "transparent")
             : (b.down ? st.panel3 : (b.hovered ? st.panel3 : st.panel2))
        border.color: b.highlighted || b.flat || b.danger ? "transparent" : st.border
        border.width: 1
    }
}
