import QtQuick

Rectangle {
    id: button

    property string text: ""
    property string icon: ""
    property bool active: true
    signal clicked()

    width: 90
    height: 60
    radius: 6
    color: active ? "#f2f2f2" : "#d8d8d8"
    border.color: "#555555"
    border.width: 2

    onActiveChanged: glyph.requestPaint()
    onIconChanged: glyph.requestPaint()
    onVisibleChanged: glyph.requestPaint()

    Text {
        visible: button.icon === ""
        anchors.centerIn: parent
        text: button.text
        font.pixelSize: 28
        color: button.active ? "#111111" : "#666666"
    }

    Canvas {
        id: glyph
        anchors.centerIn: parent
        width: Math.min(parent.width - 16, parent.height - 12)
        height: width
        visible: button.icon !== ""

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = button.active ? "#111111" : "#666666"
            ctx.fillStyle = ctx.strokeStyle
            ctx.lineWidth = 4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2

            function arrowHead(x, y, direction, size) {
                ctx.beginPath()
                ctx.moveTo(x + size * Math.cos(direction), y + size * Math.sin(direction))
                ctx.lineTo(x + size * Math.cos(direction + 2.5), y + size * Math.sin(direction + 2.5))
                ctx.lineTo(x + size * Math.cos(direction - 2.5), y + size * Math.sin(direction - 2.5))
                ctx.closePath()
                ctx.fill()
            }

            if (button.icon === "left" || button.icon === "right") {
                ctx.lineWidth = 5
                ctx.beginPath()
                if (button.icon === "left") {
                    ctx.moveTo(w * 0.68, h * 0.18)
                    ctx.lineTo(w * 0.28, cy)
                    ctx.lineTo(w * 0.68, h * 0.82)
                } else {
                    ctx.moveTo(w * 0.32, h * 0.18)
                    ctx.lineTo(w * 0.72, cy)
                    ctx.lineTo(w * 0.32, h * 0.82)
                }
                ctx.stroke()
            } else if (button.icon === "down") {
                var shaftWidth = Math.max(6, w * 0.14)
                var shaftHeight = h * 0.42
                ctx.fillRect(cx - shaftWidth / 2, h * 0.14, shaftWidth, shaftHeight)
                var headHeight = h * 0.34
                var rows = Math.max(3, Math.round(headHeight))
                var headTop = h * 0.5
                var halfBase = w * 0.34
                for (var row = 0; row < rows; ++row) {
                    var half = halfBase * (rows - 1 - row) / Math.max(1, rows - 1)
                    ctx.fillRect(cx - half, headTop + row * headHeight / rows,
                                 half * 2 + 1, headHeight / rows + 1)
                }
            } else if (button.icon === "home") {
                ctx.beginPath()
                ctx.moveTo(w * 0.08, h * 0.5)
                ctx.lineTo(cx, h * 0.14)
                ctx.lineTo(w * 0.92, h * 0.5)
                ctx.stroke()
                ctx.strokeRect(w * 0.22, h * 0.48, w * 0.56, h * 0.4)
            } else if (button.icon === "refresh") {
                var radius = w * 0.34
                var firstEnd = Math.PI * 0.35
                var secondEnd = Math.PI * 1.35
                ctx.beginPath()
                ctx.arc(cx, cy, radius, -Math.PI * 0.8, firstEnd)
                ctx.stroke()
                arrowHead(cx + radius * Math.cos(firstEnd), cy + radius * Math.sin(firstEnd),
                          firstEnd + Math.PI / 2, 9)
                ctx.beginPath()
                ctx.arc(cx, cy, radius, Math.PI * 0.2, secondEnd)
                ctx.stroke()
                arrowHead(cx + radius * Math.cos(secondEnd), cy + radius * Math.sin(secondEnd),
                          secondEnd + Math.PI / 2, 9)
            } else if (button.icon === "account") {
                ctx.beginPath()
                ctx.arc(cx, h * 0.3, w * 0.16, 0, 2 * Math.PI)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(cx, h * 0.9, w * 0.32, Math.PI, 0, true)
                ctx.stroke()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: button.active
        onClicked: button.clicked()
    }
}
