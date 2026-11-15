import QtQuick 2.2
import Sailfish.Silica 1.0

Item {
    id: row

    property string label
    property string value
    property bool interactive: false

    signal clicked()

    width: parent ? parent.width : 0
    height: Math.max(labelLabel.height, valueLabel.height) + Theme.paddingSmall

    Label {
        id: labelLabel
        x: Theme.horizontalPageMargin
        width: parent.width / 2 - Theme.horizontalPageMargin
        text: row.label
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeSmall
        truncationMode: TruncationMode.Fade
    }

    Label {
        id: valueLabel
        anchors.right: parent.right
        anchors.rightMargin: Theme.horizontalPageMargin
        width: parent.width / 2 - Theme.horizontalPageMargin
        horizontalAlignment: Text.AlignRight
        text: row.value
        color: row.interactive ? Theme.highlightColor : Theme.primaryColor
        font.pixelSize: Theme.fontSizeSmall
        truncationMode: TruncationMode.Fade
    }

    MouseArea {
        anchors.fill: parent
        enabled: row.interactive
        onClicked: row.clicked()
    }
}
