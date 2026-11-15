import QtQuick 2.2
import Sailfish.Silica 1.0

CoverBackground {
    id: cover

    property int now: Math.floor(Date.now() / 1000)

    function ageText() {
        if (trackclient.lastPointTs <= 0) {
            return qsTr("sin puntos")
        }
        var minutes = Math.round((now - trackclient.lastPointTs) / 60)
        if (minutes < 1) {
            return qsTr("ahora")
        }
        if (minutes < 60) {
            return qsTr("hace %1 min").arg(minutes)
        }
        return qsTr("hace %1 h").arg(Math.round(minutes / 60))
    }

    Timer {
        interval: 60000
        repeat: true
        running: true
        onTriggered: cover.now = Math.floor(Date.now() / 1000)
    }

    Column {
        anchors.centerIn: parent
        spacing: Theme.paddingSmall

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "TrackLog"
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.primaryColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: trackclient.tracking
                  ? (trackclient.stationary ? qsTr("Parado") : qsTr("Grabando"))
                  : qsTr("En pausa")
            font.pixelSize: Theme.fontSizeMedium
            color: trackclient.tracking ? Theme.highlightColor : Theme.secondaryColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: cover.ageText()
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: trackclient.pointsToday + qsTr(" puntos hoy")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: trackclient.tracking
                        ? "image://theme/icon-cover-pause"
                        : "image://theme/icon-cover-play"
            onTriggered: {
                if (trackclient.tracking) {
                    trackclient.stopTracking()
                } else {
                    trackclient.startTracking()
                }
            }
        }
    }
}
