import QtQuick 2.2
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    property date fromDate: new Date()
    property date toDate: new Date()

    SilicaListView {
        anchors.fill: parent
        model: dayhistory.points

        header: PageHeader {
            title: Qt.formatDate(page.fromDate, "dd/MM/yyyy")
        }

        delegate: ListItem {
            contentHeight: Theme.itemSizeMedium

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall / 2

                Label {
                    text: Qt.formatTime(new Date(modelData.ts * 1000), "hh:mm:ss")
                          + (modelData.speed > 0.5
                             ? "  ·  " + Math.round(modelData.speed) + " km/h"
                             : "")
                    font.pixelSize: Theme.fontSizeSmall
                }

                Label {
                    text: (modelData.acc >= 0
                           ? qsTr("precisión ") + Math.round(modelData.acc) + " m"
                           : qsTr("precisión desconocida"))
                          + (modelData.uploaded ? qsTr(" · subido") : qsTr(" · pendiente"))
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: dayhistory.points.length === 0
            text: qsTr("Sin puntos en este día")
        }
    }

    Component.onCompleted: {
        var from = new Date(page.fromDate)
        from.setHours(0, 0, 0, 0)
        var to = new Date(page.toDate)
        to.setHours(23, 59, 59, 0)
        dayhistory.setRangeMs(from.getTime(), to.getTime())
    }
}
