import QtQuick 2.2
import Sailfish.Silica 1.0

Canvas {
    id: chart

    property var points: []
    property string valueSuffix: "km/h"

    height: Theme.itemSizeLarge * 2

    onPointsChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function maxSpeed() {
        var max = 0
        for (var i = 0; i < (points ? points.length : 0); ++i) {
            if (points[i].speed > max) {
                max = points[i].speed
            }
        }
        return Math.max(10, Math.ceil(max / 10) * 10)
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        if (!points || points.length < 2) {
            return
        }

        var top = maxSpeed()
        var dx = width / (points.length - 1)

        ctx.beginPath()
        ctx.moveTo(0, height)
        for (var i = 0; i < points.length; ++i) {
            var y = height - Math.min(points[i].speed / top, 1.0) * (height - 2)
            ctx.lineTo(i * dx, y)
        }
        ctx.lineTo(width, height)
        ctx.closePath()
        ctx.fillStyle = Theme.rgba(Theme.highlightColor, 0.25)
        ctx.fill()

        ctx.beginPath()
        for (var j = 0; j < points.length; ++j) {
            var yy = height - Math.min(points[j].speed / top, 1.0) * (height - 2)
            if (j === 0) {
                ctx.moveTo(0, yy)
            } else {
                ctx.lineTo(j * dx, yy)
            }
        }
        ctx.strokeStyle = Theme.highlightColor
        ctx.lineWidth = 2
        ctx.stroke()

        ctx.fillStyle = Theme.secondaryColor
        ctx.font = Theme.fontSizeExtraSmall + "px sans-serif"
        ctx.fillText(Math.round(top) + " " + valueSuffix, 4, 14)
    }
}
