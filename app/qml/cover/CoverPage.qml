import QtQuick 2.2
import Sailfish.Silica 1.0

CoverBackground {
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
            text: trackclient.tracking ? qsTr("Grabando") : qsTr("En pausa")
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.secondaryColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: trackclient.pointsToday + qsTr(" puntos hoy")
            font.pixelSize: Theme.fontSizeSmall
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
