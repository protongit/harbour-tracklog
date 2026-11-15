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

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Actualizar")
                onClicked: history.reload()
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

            MapView {
                id: map
                width: parent.width
                height: Math.max(Theme.itemSizeLarge * 4, page.height * 0.42)
                points: history.points
            }

            DetailRow { label: qsTr("Periodo"); value: history.rangeText }
            DetailRow { label: qsTr("Puntos"); value: history.pointCount }
            DetailRow {
                label: qsTr("Distancia")
                value: history.distanceKm.toFixed(2) + " km"
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Exportar GPX")
                enabled: !history.empty
                onClicked: page.exportPath = history.exportGpx()
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
                }
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

    Component.onCompleted: {
        page.ready = true
        page.applyPreset(1)
    }
}
