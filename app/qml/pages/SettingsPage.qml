import QtQuick 2.2
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property var intervals: [30, 60, 120, 300]

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Ajustes") }

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
                placeholderText: "jollaphone2026"
                inputMethodHints: Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: uploadSwitch.focus = true
            }

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
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: distanceField.focus = true
            }

            TextField {
                id: distanceField
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                label: qsTr("Distancia mínima (m)")
                inputMethodHints: Qt.ImhDigitsOnly
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

            TextSwitch {
                id: autoStartSwitch
                text: qsTr("Grabar al iniciar")
                description: qsTr("Empieza a grabar cuando arranca el teléfono")
            }

            Button {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                text: qsTr("Guardar")
                onClicked: {
                    trackclient.applyConfig({
                        "url": urlField.text,
                        "id": idField.text,
                        "uploadEnabled": uploadSwitch.checked,
                        "traccar": traccarSwitch.checked,
                        "autoStart": autoStartSwitch.checked,
                        "intervalSec": page.intervals[intervalCombo.currentIndex],
                        "minAccuracyM": parseInt(accuracyField.text) || 120,
                        "minDistanceM": parseInt(distanceField.text) || 0
                    })
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

        var index = page.intervals.indexOf(trackclient.intervalSec)
        intervalCombo.currentIndex = index >= 0 ? index : 1
    }
}
