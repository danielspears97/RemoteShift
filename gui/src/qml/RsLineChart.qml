import QtQuick
import QtQuick.Controls

// Compact history chart for one measured series (one sample per second, newest on the right).
Rectangle {
    id: chart
    property string title
    property string unit
    property var values: []
    property int decimals: 0
    property real floorMax: 1          // y axis never shrinks below this
    property real target: -1           // optional reference line
    property color lineColor: st.accent
    property string valueText: values.length ? Number(values[values.length - 1]).toFixed(decimals) + (unit ? " " + unit : "") : "—"
    property string note: ""

    implicitHeight: 150
    radius: st.radius
    color: st.panel
    border.color: st.border
    RsStyle { id: st }

    readonly property real maxValue: {
        let m = floorMax;
        for (let i = 0; i < values.length; i++)
            if (values[i] > m) m = values[i];
        if (target > m) m = target;
        return m * 1.12;
    }
    onValuesChanged: cv.requestPaint()
    onWidthChanged: cv.requestPaint()
    onHeightChanged: cv.requestPaint()

    Label {
        id: titleLabel
        anchors { left: parent.left; top: parent.top; margins: 12 }
        text: chart.title
        color: st.textDim
        font.pixelSize: st.fontSmall
    }
    Label {
        anchors { right: parent.right; top: parent.top; margins: 10 }
        text: chart.valueText
        color: st.text
        font.pixelSize: 18
        font.bold: true
    }
    Label {
        visible: chart.note.length > 0
        anchors { left: titleLabel.right; verticalCenter: titleLabel.verticalCenter; leftMargin: 8 }
        text: chart.note
        color: st.textFaint
        font.pixelSize: 11
    }
    Canvas {
        id: cv
        anchors { fill: parent; topMargin: 40; leftMargin: 12; rightMargin: 12; bottomMargin: 10 }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const w = width, h = height, n = chart.values.length;
            ctx.strokeStyle = "#262a33";
            ctx.lineWidth = 1;
            for (let i = 0; i <= 2; i++) {
                const y = Math.round(h * i / 2) + 0.5;
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
            }
            const yOf = (v) => h - (v / chart.maxValue) * h;
            if (chart.target > 0) {
                ctx.strokeStyle = "#4a5060";
                ctx.setLineDash([4, 4]);
                ctx.beginPath(); ctx.moveTo(0, yOf(chart.target)); ctx.lineTo(w, yOf(chart.target)); ctx.stroke();
                ctx.setLineDash([]);
            }
            if (n < 2)
                return;
            // Show at most the last 5 minutes; fewer samples stretch from the right edge.
            const span = 300;
            const from = Math.max(0, n - span);
            const count = n - from;
            const xOf = (i) => w - (count - 1 - i) * (w / (span - 1));
            ctx.beginPath();
            ctx.moveTo(xOf(0), yOf(chart.values[from]));
            for (let i = 1; i < count; i++)
                ctx.lineTo(xOf(i), yOf(chart.values[from + i]));
            ctx.strokeStyle = chart.lineColor;
            ctx.lineWidth = 2;
            ctx.stroke();
            ctx.lineTo(xOf(count - 1), h);
            ctx.lineTo(xOf(0), h);
            ctx.closePath();
            ctx.globalAlpha = 0.13;
            ctx.fillStyle = chart.lineColor;
            ctx.fill();
            ctx.globalAlpha = 1;
        }
    }
}
