import QtQuick 2.2
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page
    allowedOrientations: Orientation.All

    property date fromDate: new Date(Date.now() - 6 * 24 * 3600 * 1000)
    property date toDate: new Date()
    property bool ready: false
    property string exportPath: ""
    property string importMessage: ""
    property string prefetchText: ""

    function applyPreset(index) {
        var now = new Date()
        var from = new Date()

        if (index === 0) {
            from.setHours(0, 0, 0, 0)
        } else if (index === 1) {
            from.setDate(now.getDate() - 6)
            from.setHours(0, 0, 0, 0)
        } else if (index === 2) {
            from.setDate(now.getDate() - 29)
            from.setHours(0, 0, 0, 0)
        } else {
            return
        }

        page.fromDate = from
        page.toDate = now
        history.setPreset(index === 0 ? 0 : (index === 1 ? 7 : 30))
    }

    function applyRange() {
        var from = new Date(page.fromDate)
        from.setHours(0, 0, 0, 0)
        var to = new Date(page.toDate)
        to.setHours(23, 59, 59, 0)
        history.setRangeMs(from.getTime(), to.getTime())
    }

    function selectDay(dateString) {
        var start = new Date(dateString + "T00:00:00")
        if (isNaN(start.getTime())) {
            return
        }
        page.fromDate = start
        page.toDate = start
        page.applyRange()
    }

    function openPoints() {
        pageStack.push(Qt.resolvedUrl("PointsPage.qml"),
                       { "fromDate": page.fromDate, "toDate": page.toDate })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Actualizar")
                onClicked: history.reload()
            }
            MenuItem {
                text: qsTr("Importar GPX")
                onClicked: importDialog.open()
            }
        }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Histórico") }

            ComboBox {
                id: presetCombo
                label: qsTr("Periodo")
                currentIndex: 1
                menu: ContextMenu {
                    MenuItem { text: qsTr("Hoy") }
                    MenuItem { text: qsTr("7 días") }
                    MenuItem { text: qsTr("30 días") }
                }
                onCurrentIndexChanged: {
                    if (page.ready) {
                        page.applyPreset(currentIndex)
                    }
                }
            }

            Row {
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingMedium

                Button {
                    text: qsTr("Desde ") + Qt.formatDate(page.fromDate, "dd/MM/yy")
                    onClicked: fromPicker.open()
                }
                Button {
                    text: qsTr("Hasta ") + Qt.formatDate(page.toDate, "dd/MM/yy")
                    onClicked: toPicker.open()
                }
            }

            Item {
                width: parent.width
                height: map.height

                MapView {
                    id: map
                    width: parent.width
                    height: Math.max(Theme.itemSizeLarge * 4, page.height * 0.42)
                    points: history.points
                }

                Column {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.paddingSmall
                    spacing: Theme.paddingSmall

                    Label {
                        anchors.right: parent.right
                        text: page.prefetchText
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.highlightColor
                    }

                    Button {
                        text: qsTr("Descargar zona")
                        font.pixelSize: Theme.fontSizeExtraSmall
                        onClicked: {
                            page.prefetchText = qsTr("descargando…")
                            tilestore.prefetch(map.centerLat, map.centerLon, map.zoom, 2)
                        }
                    }
                }
            }

            Row {
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingMedium

                Button {
                    text: qsTr("Ver puntos")
                    onClicked: page.openPoints()
                }
                Button {
                    text: qsTr("Exportar GPX")
                    enabled: !history.empty
                    onClicked: page.exportPath = history.exportGpx()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.exportPath.length > 0
                text: page.exportPath.length > 0
                      ? qsTr("GPX guardado en:\n") + page.exportPath
                      : ""
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            DetailRow { label: qsTr("Periodo"); value: history.rangeText }
            DetailRow { label: qsTr("Puntos"); value: history.pointCount }
            DetailRow {
                label: qsTr("Distancia")
                value: history.distanceKm.toFixed(2) + " km"
            }
            DetailRow {
                label: qsTr("Velocidad media")
                value: history.avgSpeedKmh.toFixed(1) + " km/h"
            }
            DetailRow {
                label: qsTr("Velocidad máxima")
                value: history.maxSpeedKmh.toFixed(1) + " km/h"
            }
            DetailRow {
                label: qsTr("Desnivel positivo")
                value: Math.round(history.ascentM) + " m"
            }

            SectionHeader { text: qsTr("Velocidad") }

            SpeedChart {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                points: history.points
            }

            SectionHeader { text: qsTr("Recorridos") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: history.trips.length === 0
                text: qsTr("Sin recorridos")
                color: Theme.secondaryColor
            }

            Repeater {
                model: history.trips

                delegate: DetailRow {
                    label: modelData.label
                    value: modelData.km.toFixed(1) + " km"
                }
            }

            SectionHeader { text: qsTr("Días") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("No hay puntos en este periodo")
                color: Theme.secondaryColor
                visible: history.empty
            }

            Repeater {
                model: history.days

                delegate: DetailRow {
                    label: modelData.date
                    value: modelData.count + qsTr(" puntos")
                    interactive: true
                    onClicked: page.selectDay(modelData.date)
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                text: qsTr("Toca un día para filtrarlo")
            }
        }
    }

    DatePickerDialog {
        id: fromPicker
        date: page.fromDate
        onAccepted: {
            page.fromDate = date
            page.applyRange()
        }
    }

    DatePickerDialog {
        id: toPicker
        date: page.toDate
        onAccepted: {
            page.toDate = date
            page.applyRange()
        }
    }

    Dialog {
        id: importDialog
        property var files: []

        onOpened: files = history.gpxFiles()
        onAccepted: close()

        Column {
            width: parent.width

            DialogHeader {
                title: qsTr("Importar GPX")
                acceptText: qsTr("Cerrar")
                cancelText: ""
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: importDialog.files.length === 0
                      ? qsTr("No hay archivos GPX en Documentos/TrackLog")
                      : qsTr("Archivos en Documentos/TrackLog")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Repeater {
                model: importDialog.files

                delegate: BackgroundItem {
                    width: parent ? parent.width : 0
                    height: Theme.itemSizeSmall

                    Label {
                        x: Theme.horizontalPageMargin
                        text: modelData.name
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    onClicked: {
                        trackclient.importGpx(modelData.path)
                        importDialog.close()
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        page.ready = true
        page.applyPreset(1)
    }

    Connections {
        target: tilestore
        onPrefetchProgress: page.prefetchText = done + " / " + total
        onPrefetchFinished: page.prefetchText = qsTr("zona descargada")
    }

    Connections {
        target: trackclient
        onImportFinished: {
            page.importMessage = count > 0
                    ? qsTr("Importados ") + count + qsTr(" puntos")
                    : qsTr("No se pudo importar")
            history.reload()
        }
    }
}
