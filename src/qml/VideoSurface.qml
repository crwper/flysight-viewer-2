import QtQuick
import QtMultimedia

Item {
    id: root

    VideoOutput {
        id: videoOutput
        objectName: "videoOutput"
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }
}
