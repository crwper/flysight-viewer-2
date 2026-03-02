import QtQuick
import QtMultimedia

Rectangle {
    id: root
    color: "black"
    signal clicked()
    signal filesDropped(var urls)

    VideoOutput {
        id: videoOutput
        objectName: "videoOutput"
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    DropArea {
        anchors.fill: parent
        onEntered: (drag) => { drag.accepted = drag.hasUrls }
        onDropped: (drop) => {
            if (drop.hasUrls) {
                root.filesDropped(drop.urls)
                drop.accepted = true
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
