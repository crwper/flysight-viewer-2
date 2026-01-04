import QtQuick
import QtLocation
import QtPositioning

Item {
    id: root

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin

        zoomLevel: 12
        center: (trackModel && trackModel.hasData) ? trackModel.center : QtPositioning.coordinate(0, 0)

        function fitToTracks() {
            if (!trackModel || !trackModel.hasData)
                return;
            map.fitViewportToGeoShape(trackModel.bounds, 40)
        }

        Component.onCompleted: fitToTracks()

        Connections {
            target: trackModel
            function onBoundsChanged() { map.fitToTracks() }
        }

        MapItemView {
            model: trackModel

            delegate: MapPolyline {
                z: 10
                line.width: 3
                line.color: trackColor

                // trackPoints is [{lat, lon}, ...]
                path: trackPoints
                      ? trackPoints.map(function(p) { return QtPositioning.coordinate(p.lat, p.lon) })
                      : []
            }
        }
    }
}
