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
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            var size = Math.min(w, h)

            if (button.icon === "left" || button.icon === "right") {
                ctx.lineWidth = Math.max(6, size / 7)
                ctx.beginPath()
                if (button.icon === "left") {
                    ctx.moveTo(w * 0.76, h * 0.16)
                    ctx.lineTo(w * 0.22, cy)
                    ctx.lineTo(w * 0.76, h * 0.84)
                } else {
                    ctx.moveTo(w * 0.24, h * 0.16)
                    ctx.lineTo(w * 0.78, cy)
                    ctx.lineTo(w * 0.24, h * 0.84)
                }
                ctx.stroke()
            } else if (button.icon === "down") {
                var shaftWidth = size / 6
                var shaftHeight = size / 2
                ctx.fillRect(cx - shaftWidth / 2, h * 0.12, shaftWidth, shaftHeight)
                var headWidth = size * 0.76
                var headHeight = size * 0.34
                var headTop = h * 0.12 + shaftHeight - headHeight / 2
                ctx.beginPath()
                ctx.moveTo(cx - headWidth / 2, headTop)
                ctx.lineTo(cx + headWidth / 2, headTop)
                ctx.lineTo(cx, headTop + headHeight)
                ctx.closePath()
                ctx.fill()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: button.active
        onClicked: button.clicked()
    }
}
