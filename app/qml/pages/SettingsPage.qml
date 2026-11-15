import QtQuick 2.2
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property var intervals: [30, 60, 120, 300]
    property var tileCodes: ["osm", "topo", "cyclosm"]
    property string testResult: ""
    property string backupMessage: ""
    property real cacheBytes: 0

    function cacheText() {
        var mb = page.cacheBytes / (1024 * 1024)
        if (mb >= 1) {
            return mb.toFixed(1) + " MB"
        }
        return Math.round(page.cacheBytes / 1024) + " KB"
    }

    function save() {
        trackclient.setTileSource(page.tileCodes[tileCombo.currentIndex])
        trackclient.applyConfig({
            "url": urlField.text,
            "id": idField.text,
            "uploadEnabled": uploadSwitch.checked,
            "traccar": traccarSwitch.checked,
            "autoStart": autoStartSwitch.checked,
            "intervalSec": page.intervals[intervalCombo.currentIndex],
            "minAccuracyM": parseInt(accuracyField.text) || 120,
            "minDistanceM": parseInt(distanceField.text) || 0,
            "pauseBelowBattery": parseInt(pauseBatteryField.text) || 0,
            "retentionDays": parseInt(retentionField.text) || 0,
            "geofenceEnabled": geofenceSwitch.checked,
            "geofenceName": geofenceNameField.text,
            "geofenceLat": parseFloat(geofenceLatField.text) || 0,
            "geofenceLon": parseFloat(geofenceLonField.text) || 0,
            "geofenceRadius": parseInt(geofenceRadiusField.text) || 200
        })
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Ajustes") }

            SectionHeader { text: qsTr("Servidor") }

            TextField {
                id: urlField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("URL del servidor")
                placeholderText: "https://servidor/apps/phonetrack/logGet/TOKEN/"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: idField.focus = true
            }

            TextField {
                id: idField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("ID del dispositivo")
                placeholderText: "tracklog"
                inputMethodHints: Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: uploadSwitch.focus = true
            }

            TextSwitch {
                id: uploadSwitch
                text: qsTr("Subir puntos")
                description: qsTr("PhoneTrack u otro servidor compatible")
            }

            TextSwitch {
                id: traccarSwitch
                text: qsTr("URL estilo Traccar")
                description: qsTr("Usa ?id= en lugar de /id/")
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Probar conexión")
                onClicked: {
                    page.testResult = qsTr("Probando…")
                    trackclient.testConnection()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.testResult.length > 0
                text: page.testResult
                font.pixelSize: Theme.fontSizeExtraSmall
                color: page.testResult.indexOf(qsTr("correcta")) >= 0
                       ? Theme.highlightColor : Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            SectionHeader { text: qsTr("Captura") }

            ComboBox {
                id: intervalCombo
                label: qsTr("Intervalo de captura")
                menu: ContextMenu {
                    MenuItem { text: "30 s" }
                    MenuItem { text: "60 s" }
                    MenuItem { text: "2 min" }
                    MenuItem { text: "5 min" }
                }
            }

            TextField {
                id: accuracyField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Precisión máxima (m)")
                inputMethodHints: Qt.ImhDigitsOnly
            }

            TextField {
                id: distanceField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Distancia mínima (m)")
                inputMethodHints: Qt.ImhDigitsOnly
            }

            TextField {
                id: pauseBatteryField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Pausar por debajo de batería (%)")
                description: qsTr("0 desactiva la pausa")
                inputMethodHints: Qt.ImhDigitsOnly
            }

            TextSwitch {
                id: autoStartSwitch
                text: qsTr("Grabar al iniciar")
                description: qsTr("Empieza a grabar cuando arranca el teléfono")
            }

            SectionHeader { text: qsTr("Datos") }

            TextField {
                id: retentionField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Conservar histórico (días)")
                description: qsTr("0 conserva todo")
                inputMethodHints: Qt.ImhDigitsOnly
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Copia de seguridad de la base de datos")
                onClicked: {
                    page.backupMessage = qsTr("Creando copia…")
                    trackclient.backupDatabase()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: page.backupMessage.length > 0
                text: page.backupMessage
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Borrar caché de mapas")
                onClicked: {
                    tilestore.clearCache()
                    page.cacheBytes = 0
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Caché de mapas: ") + page.cacheText()
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
            }

            SectionHeader { text: qsTr("Mapa") }

            ComboBox {
                id: tileCombo
                label: qsTr("Fuente de tiles")
                menu: ContextMenu {
                    MenuItem { text: "OpenStreetMap" }
                    MenuItem { text: "OpenTopoMap" }
                    MenuItem { text: "CyclOSM" }
                }
            }

            SectionHeader { text: qsTr("Geocerca") }

            TextSwitch {
                id: geofenceSwitch
                text: qsTr("Avisar al llegar o salir")
                description: qsTr("Notificación al entrar o salir del radio")
            }

            TextField {
                id: geofenceNameField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Nombre del lugar")
            }

            TextField {
                id: geofenceLatField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Latitud")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            TextField {
                id: geofenceLonField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Longitud")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            TextField {
                id: geofenceRadiusField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Radio (m)")
                inputMethodHints: Qt.ImhDigitsOnly
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Guardar")
                onClicked: {
                    page.save()
                    pageStack.pop()
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                text: qsTr("Base de datos: ") + trackclient.databasePath
            }
        }
    }

    Component.onCompleted: {
        urlField.text = trackclient.serverUrl
        idField.text = trackclient.deviceId
        uploadSwitch.checked = trackclient.uploadEnabled
        traccarSwitch.checked = trackclient.traccar
        autoStartSwitch.checked = trackclient.autoStart
        accuracyField.text = trackclient.minAccuracyM > 0 ? trackclient.minAccuracyM : 120
        distanceField.text = trackclient.minDistanceM > 0 ? trackclient.minDistanceM : 0
        pauseBatteryField.text = trackclient.pauseBelowBattery
        retentionField.text = trackclient.retentionDays
        geofenceSwitch.checked = trackclient.geofenceEnabled
        geofenceNameField.text = trackclient.geofenceName
        geofenceLatField.text = trackclient.geofenceLat
        geofenceLonField.text = trackclient.geofenceLon
        geofenceRadiusField.text = trackclient.geofenceRadius
        page.cacheBytes = tilestore.cacheSizeBytes()

        var index = page.intervals.indexOf(trackclient.intervalSec)
        intervalCombo.currentIndex = index >= 0 ? index : 1
        var tileIndex = page.tileCodes.indexOf(trackclient.tileSource)
        tileCombo.currentIndex = tileIndex >= 0 ? tileIndex : 0
    }

    Connections {
        target: trackclient
        onTestFinished: {
            page.testResult = result.ok
                    ? qsTr("Conexión correcta (HTTP ") + result.code + ")"
                    : (result.error.length > 0
                       ? result.error
                       : qsTr("Error de conexión"))
        }
        onBackupFinished: {
            page.backupMessage = path.length > 0
                    ? qsTr("Copia guardada en:\n") + path
                    : qsTr("No se pudo crear la copia")
        }
    }
}
