import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// Rounded panel with an optional title, used to group settings.
Rectangle {
    id: card
    property string title
    property string subtitle
    default property alias content: body.data
    property alias bodySpacing: body.spacing
    implicitHeight: col.implicitHeight + 32
    radius: st.radius
    color: st.panel
    border.color: st.border
    border.width: 1
    RsStyle { id: st }

    ColumnLayout {
        id: col
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 16
        }
        spacing: 10
        ColumnLayout {
            visible: card.title.length > 0
            spacing: 2
            Label {
                text: card.title
                color: st.text
                font.pixelSize: 16
                font.bold: true
            }
            Label {
                visible: card.subtitle.length > 0
                text: card.subtitle
                color: st.textDim
                font.pixelSize: st.fontSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 8
        }
    }
}
