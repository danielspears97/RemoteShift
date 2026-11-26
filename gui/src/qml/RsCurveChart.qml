import QtQuick
import QtQuick.Controls

// Plots mouse speed (normalised) -> stick deflection for the active mode, with the live
// operating point and draggable points for custom curves.
Item {
    id: chart
    property var points: []          // [Qt.point(in, out)]
    property var customPoints: []    // [[in, out]] when curve == "custom"
    property bool editable: false
    property real liveInput: -1      // normalised input magnitude, -1 = hidden
    signal customPointsEdited(var pts)

    implicitWidth: 360
    implicitHeight: 240
    RsStyle { id: st }

    readonly property real pad: 28
    function px(v) { return pad + v * (width - pad - 10); }
    function py(v) { return height - pad - v * (height - pad - 10); }
    function sampleAt(x) {
        if (!points.length) return 0;
        const n = points.length - 1;
        const i = Math.max(0, Math.min(n, Math.round(x * n)));
        return points[i].y;
    }

    onPointsChanged: cv.requestPaint()
    onLiveInputChanged: cv.requestPaint()
    onWidthChanged: cv.requestPaint()
    onHeightChanged: cv.requestPaint()

    Rectangle {
        anchors.fill: parent
        radius: st.radius
        color: "#15171c"
        border.color: st.border
    }

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.lineWidth = 1;
            ctx.strokeStyle = "#262a33";
            for (let i = 0; i <= 4; i++) {
                const g = i / 4;
                ctx.beginPath(); ctx.moveTo(px(g), py(0)); ctx.lineTo(px(g), py(1)); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(px(0), py(g)); ctx.lineTo(px(1), py(g)); ctx.stroke();
            }
            // identity reference
            ctx.strokeStyle = "#3a3f4c";
            ctx.setLineDash([4, 4]);
            ctx.beginPath(); ctx.moveTo(px(0), py(0)); ctx.lineTo(px(1), py(1)); ctx.stroke();
            ctx.setLineDash([]);
            if (chart.points.length) {
                ctx.lineWidth = 2.5;
                ctx.strokeStyle = st.accent;
                ctx.beginPath();
                ctx.moveTo(px(chart.points[0].x), py(chart.points[0].y));
                for (let i = 1; i < chart.points.length; i++)
                    ctx.lineTo(px(chart.points[i].x), py(chart.points[i].y));
                ctx.stroke();
            }
            if (chart.liveInput >= 0) {
                const x = Math.min(1, chart.liveInput);
                ctx.fillStyle = st.accent2;
                ctx.beginPath();
                ctx.arc(px(x), py(sampleAt(x)), 5, 0, Math.PI * 2);
                ctx.fill();
            }
            ctx.fillStyle = st.textFaint;
            ctx.font = "11px sans-serif";
            ctx.fillText(qsTr("mouse speed →"), px(0.62), height - 8);
            ctx.save();
            ctx.translate(12, py(0.25));
            ctx.rotate(-Math.PI / 2);
            ctx.fillText(qsTr("stick →"), 0, 0);
            ctx.restore();
        }
    }

    Repeater {
        model: chart.editable ? chart.customPoints.length : 0
        delegate: Rectangle {
            id: handle
            required property int index
            width: 14; height: 14; radius: 7
            color: st.accent2
            border.color: "white"
            x: chart.px(chart.customPoints[index][0]) - width / 2
            y: chart.py(chart.customPoints[index][1]) - height / 2
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                cursorShape: Qt.SizeAllCursor
                property var pts
                onPressed: pts = chart.customPoints.map(p => [p[0], p[1]])
                onPositionChanged: (mouse) => {
                    const p = mapToItem(chart, mouse.x, mouse.y);
                    const inV = Math.max(0.01, Math.min(0.99, (p.x - chart.pad) / (chart.width - chart.pad - 10)));
                    const outV = Math.max(0, Math.min(1, (chart.height - chart.pad - p.y) / (chart.height - chart.pad - 10)));
                    pts[handle.index] = [inV, outV];
                    chart.customPointsEdited(pts);
                }
            }
        }
    }
}
