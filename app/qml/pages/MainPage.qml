import QtQuick 2.2
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Ajustes")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                text: qsTr("Actualizar")
                onClicked: trackclient.refresh()
            }
        }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("TrackLog") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraLarge
                color: !trackclient.serviceRunning
                       ? Theme.secondaryColor
                       : (trackclient.tracking ? Theme.highlightColor : Theme.secondaryHighlightColor)
                text: !trackclient.serviceRunning
                      ? qsTr("Servicio parado")
                      : (trackclient.tracking ? qsTr("Grabando") : qsTr("En pausa"))
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                text: trackclient.statusText
            }

            TextSwitch {
                text: qsTr("Grabar trayectoria")
                description: qsTr("El demonio sigue grabando con la app cerrada")
                checked: trackclient.tracking
                automaticCheck: false
                enabled: trackclient.serviceRunning
                onClicked: {
                    if (trackclient.tracking) {
                        trackclient.stopTracking()
                    } else {
                        trackclient.startTracking()
                    }
                }
            }

            SectionHeader { text: qsTr("Resumen") }

            DetailRow {
                label: qsTr("GPS")
                value: !trackclient.fixing
                       ? qsTr("sin fix")
                       : (trackclient.accuracy > 0
                          ? qsTr("fix") + " · " + Math.round(trackclient.accuracy) + " m"
                          : qsTr("fix"))
            }
            DetailRow {
                label: qsTr("Puntos hoy")
                value: trackclient.pointsToday
            }
            DetailRow {
                label: qsTr("Subidos hoy")
                value: trackclient.uploadedToday
            }
            DetailRow {
                label: qsTr("Pendientes de subir")
                value: trackclient.pending
            }
            DetailRow {
                label: qsTr("Intervalo")
                value: trackclient.intervalSec + " s"
            }
            DetailRow {
                label: qsTr("Capturados / ignorados")
                value: trackclient.recorded + " / " + trackclient.ignored
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                color: trackclient.lastError.length > 0 ? Theme.errorColor : Theme.secondaryColor
                wrapMode: Text.Wrap
                visible: trackclient.lastError.length > 0
                text: qsTr("Último error de subida: ") + trackclient.lastError
            }

            ButtonLayout {
                Button {
                    text: qsTr("Actualizar")
                    onClicked: trackclient.refresh()
                }
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: trackclient.serviceRunning
                      ? qsTr("Parar servicio")
                      : qsTr("Arrancar servicio")
                onClicked: {
                    if (trackclient.serviceRunning) {
                        trackclient.stopService()
                    } else {
                        trackclient.startService()
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                text: trackclient.databasePath
            }
        }
    }
}
