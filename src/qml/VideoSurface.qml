import QtQuick
import QtMultimedia

Item {
    id: root
    signal clicked()

    VideoOutput {
        id: videoOutput
        objectName: "videoOutput"
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
