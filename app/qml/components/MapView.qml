import QtQuick 2.2
import Sailfish.Silica 1.0

Item {
    id: map

    property real centerLat: 43.36
    property real centerLon: -8.25
    property int zoom: 13
    property var points: []
    property int tileVersion: 0
    property var tiles: []

    readonly property int tileSize: 256
    readonly property bool ready: width > 0 && height > 0

    clip: true

    function worldSize() { return Math.pow(2, zoom) * tileSize }
    function worldSizeAt(z) { return Math.pow(2, z) * tileSize }

    function lonToX(lon) { return (lon + 180) / 360 * worldSize() }
    function latToY(lat) {
        var s = Math.sin(lat * Math.PI / 180)
        return (0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI)) * worldSize()
    }
    function xToLon(x) { return x / worldSize() * 360 - 180 }
    function yToLat(y) {
        var n = Math.PI - 2 * Math.PI * y / worldSize()
        return 180 / Math.PI * Math.atan(0.5 * (Math.exp(n) - Math.exp(-n)))
    }

    function updateTiles() {
        if (!ready) {
            return
        }
        var cx = lonToX(centerLon)
        var cy = latToY(centerLat)
        var left = cx - width / 2
        var top = cy - height / 2
        var count = Math.pow(2, zoom)

        var x0 = Math.floor(left / tileSize)
        var y0 = Math.floor(top / tileSize)
        var x1 = Math.floor((left + width) / tileSize)
        var y1 = Math.floor((top + height) / tileSize)

        var list = []
        for (var tx = x0; tx <= x1; ++tx) {
            for (var ty = y0; ty <= y1; ++ty) {
                if (ty < 0 || ty >= count) {
                    continue
                }
                var wrapped = ((tx % count) + count) % count
                list.push({
                    "tx": tx,
                    "ty": ty,
                    "id": zoom + "/" + wrapped + "/" + ty
                })
            }
        }
        tiles = list
    }

    function panBy(dx, dy) {
        var cx = lonToX(centerLon) - dx
        var cy = latToY(centerLat) - dy
        centerLon = xToLon(cx)
        centerLat = yToLat(cy)
    }

    function setZoom(value) {
        var z = Math.max(3, Math.min(18, value))
        if (z === zoom) {
            return
        }
        zoom = z
        updateTiles()
        canvas.requestPaint()
    }

    function fitPoints(list) {
        if (!list || list.length === 0) {
            return
        }
        var minLat = 90, maxLat = -90, minLon = 180, maxLon = -180
        for (var i = 0; i < list.length; ++i) {
            var p = list[i]
            if (p.lat < minLat) minLat = p.lat
            if (p.lat > maxLat) maxLat = p.lat
            if (p.lon < minLon) minLon = p.lon
            if (p.lon > maxLon) maxLon = p.lon
        }
        centerLat = (minLat + maxLat) / 2
        centerLon = (minLon + maxLon) / 2

        var spanLon = Math.max(0.0005, maxLon - minLon)
        var spanLat = Math.max(0.0005, maxLat - minLat)
        var z = 17
        while (z > 3) {
            var w = worldSizeAt(z)
            if ((spanLon / 360) * w < width * 0.8
                    && (spanLat / 180) * w < height * 0.8) {
                break
            }
            --z
        }
        zoom = z
        updateTiles()
        canvas.requestPaint()
    }

    onWidthChanged: updateTiles()
    onHeightChanged: updateTiles()
    onZoomChanged: {
        updateTiles()
        canvas.requestPaint()
    }
    onCenterLatChanged: canvas.requestPaint()
    onCenterLonChanged: canvas.requestPaint()
    onPointsChanged: {
        canvas.requestPaint()
        if (points && points.length > 0) {
            fitPoints(points)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#15181c"
    }

    Item {
        id: world
        x: map.width / 2 - map.lonToX(map.centerLon)
        y: map.height / 2 - map.latToY(map.centerLat)

        Repeater {
            model: map.tiles

            delegate: Image {
                x: modelData.tx * map.tileSize
                y: modelData.ty * map.tileSize
                width: map.tileSize
                height: map.tileSize
                asynchronous: true
                cache: false
                source: "image://tiles/" + modelData.id + "?v=" + map.tileVersion
            }
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!map.points || map.points.length === 0) {
                return
            }

            var cx = map.lonToX(map.centerLon)
            var cy = map.latToY(map.centerLat)
            var ox = map.width / 2 - cx
            var oy = map.height / 2 - cy

            if (map.points.length > 1) {
                ctx.strokeStyle = Theme.highlightColor
                ctx.lineWidth = 4
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.beginPath()
                for (var i = 0; i < map.points.length; ++i) {
                    var p = map.points[i]
                    var x = map.lonToX(p.lon) + ox
                    var y = map.latToY(p.lat) + oy
                    if (i === 0) {
                        ctx.moveTo(x, y)
                    } else {
                        ctx.lineTo(x, y)
                    }
                }
                ctx.stroke()
            }

            var first = map.points[0]
            var last = map.points[map.points.length - 1]

            ctx.fillStyle = "#4caf50"
            ctx.beginPath()
            ctx.arc(map.lonToX(first.lon) + ox, map.latToY(first.lat) + oy, 7, 0, 2 * Math.PI)
            ctx.fill()

            ctx.fillStyle = "#f44336"
            ctx.beginPath()
            ctx.arc(map.lonToX(last.lon) + ox, map.latToY(last.lat) + oy, 7, 0, 2 * Math.PI)
            ctx.fill()
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        property real lastX: 0
        property real lastY: 0

        onPressed: {
            lastX = mouse.x
            lastY = mouse.y
        }
        onPositionChanged: {
            if (!pressed) {
                return
            }
            var dx = mouse.x - lastX
            var dy = mouse.y - lastY
            lastX = mouse.x
            lastY = mouse.y
            map.panBy(dx, dy)
        }
        onReleased: map.updateTiles()
        onCanceled: map.updateTiles()
    }

    Column {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.paddingSmall
        spacing: Theme.paddingSmall

        IconButton {
            icon.source: "image://theme/icon-m-add"
            onClicked: map.setZoom(map.zoom + 1)
        }
        IconButton {
            icon.source: "image://theme/icon-m-remove"
            onClicked: map.setZoom(map.zoom - 1)
        }
    }

    Label {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.paddingSmall
        text: "© OpenStreetMap"
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }

    Connections {
        target: tilestore
        onTileReady: map.tileVersion++
    }
}
