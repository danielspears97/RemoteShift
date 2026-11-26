import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

// Labelled slider that tracks an external value without losing its binding when dragged.
RowLayout {
    id: row
    property string label
    property string hint
    property real source: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property int decimals: 2
    property string suffix: ""
    property real displayScale: 1
    signal committed(real value)

    spacing: 12
    RsStyle { id: st }

    function syncFromSource() {
        if (!slider.pressed)
            slider.value = source;
    }
    onSourceChanged: syncFromSource()
    Component.onCompleted: syncFromSource()

    ColumnLayout {
        Layout.preferredWidth: 190
        Layout.minimumWidth: 150
        spacing: 0
        Label {
            text: row.label
            color: st.text
            font.pixelSize: st.fontBody
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            visible: row.hint.length > 0
            text: row.hint
            color: st.textFaint
            font.pixelSize: st.fontSmall
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        from: row.from
        to: row.to
        stepSize: row.stepSize
        snapMode: Slider.SnapAlways
        onMoved: row.committed(value)
        Material.accent: st.accent
    }
    Label {
        Layout.preferredWidth: 70
        horizontalAlignment: Text.AlignRight
        text: (slider.value * row.displayScale).toFixed(row.decimals) + row.suffix
        color: st.textDim
        font.pixelSize: st.fontBody
        font.family: "monospace"
    }
}
