import QtQuick
import QtLocation
import QtPositioning

Item {
    id: root

    // --- Tuning knobs (match spec defaults-ish) ---
    property real hoverThresholdPx: 10        // distance from polyline in screen px
    property real hoverMinMovePx: 2           // ignore tiny mouse jitter
    property real hoverTimeEpsilonSec: 0.02   // avoid spamming CursorModel

    // --- Internal state for hover ---
    property var   _trackItems: []                          // registered MapPolyline delegates
    property point _pendingMousePos: Qt.point(-1, -1)
    property point _lastProcessedMousePos: Qt.point(-1, -1)
    property bool  _forceHoverRecalc: false
    property string _hoverSessionId: ""
    property real   _hoverUtcSeconds: NaN

    function _cursorProxy() {
        // mapCursorProxy is expected as a context property from C++
        return (typeof mapCursorProxy !== "undefined") ? mapCursorProxy : null
    }

    function _registerTrack(item) {
        if (!item)
            return;
        if (_trackItems.indexOf(item) === -1)
            _trackItems.push(item);
    }

    function _unregisterTrack(item) {
        var i = _trackItems.indexOf(item);
        if (i !== -1)
            _trackItems.splice(i, 1);
    }

    // Returns { dist2, u } where u in [0..1] is the segment parameter of closest point.
    function _distPointToSegment2(p, a, b) {
        var abx = b.x - a.x;
        var aby = b.y - a.y;
        var apx = p.x - a.x;
        var apy = p.y - a.y;

        var denom = abx*abx + aby*aby;
        var u = 0.0;
        if (denom > 1e-9)
            u = (apx*abx + apy*aby) / denom;

        if (u < 0) u = 0;
        else if (u > 1) u = 1;

        var cx = a.x + u * abx;
        var cy = a.y + u * aby;
        var dx = p.x - cx;
        var dy = p.y - cy;

        return { dist2: dx*dx + dy*dy, u: u };
    }

    function _requestHoverUpdate(x, y) {
        _pendingMousePos = Qt.point(x, y);
        if (!hoverTimer.running)
            hoverTimer.start();
    }

    function _pokeHoverRecalc() {
        if (_pendingMousePos.x < 0 || _pendingMousePos.y < 0)
            return;
        _forceHoverRecalc = true;
        if (!hoverTimer.running)
            hoverTimer.start();
    }

    function _setMapHover(sessionId, utcSeconds) {
        var proxy = _cursorProxy();
        if (!proxy)
            return;

        var sessionChanged = (sessionId !== _hoverSessionId);
        var timeChanged = sessionChanged
                       || isNaN(_hoverUtcSeconds)
                       || Math.abs(utcSeconds - _hoverUtcSeconds) > hoverTimeEpsilonSec;

        if (!timeChanged)
            return;

        _hoverSessionId = sessionId;
        _hoverUtcSeconds = utcSeconds;

        // Expected C++ invokable:
        //   setMapHover(QString sessionId, double utcSeconds)
        proxy.setMapHover(sessionId, utcSeconds);
    }

    function _clearMapHover() {
        var proxy = _cursorProxy();

        if (_hoverSessionId === "" && isNaN(_hoverUtcSeconds))
            return;

        _hoverSessionId = "";
        _hoverUtcSeconds = NaN;

        if (proxy) {
            // Expected C++ invokable:
            //   clearMapHover()
            proxy.clearMapHover();
        }
    }

    function _updateHoverAt(mousePt) {
        var proxy = _cursorProxy();
        if (!proxy)
            return;

        var thr2 = hoverThresholdPx * hoverThresholdPx;
        var bestDist2 = thr2 + 1;
        var bestSessionId = "";
        var bestUtc = NaN;

        // Brute force over simplified tracks in screen space.
        for (var ti = 0; ti < _trackItems.length; ++ti) {
            var track = _trackItems[ti];
            var pts = track._trackPoints;
            if (!pts || pts.length < 2)
                continue;

            var sid = track._sessionId;
            if (!sid)
                continue;

            for (var i = 0; i < pts.length - 1; ++i) {
                var p0 = pts[i];
                var p1 = pts[i + 1];

                // Requires time-per-point for map->plot cursor driving:
                // trackPoints = [{lat, lon, t}, ...] where t is UTC seconds.
                if (p0.t === undefined || p1.t === undefined)
                    continue;

                var A = map.fromCoordinate(QtPositioning.coordinate(p0.lat, p0.lon), false);
                var B = map.fromCoordinate(QtPositioning.coordinate(p1.lat, p1.lon), false);
                var r = _distPointToSegment2(mousePt, A, B);

                if (r.dist2 < bestDist2) {
                    bestDist2 = r.dist2;
                    bestSessionId = sid;
                    bestUtc = p0.t + r.u * (p1.t - p0.t);
                }
            }
        }

        if (bestSessionId !== "" && bestDist2 <= thr2 && !isNaN(bestUtc)) {
            _setMapHover(bestSessionId, bestUtc);
        } else {
            _clearMapHover();
        }
    }

    Timer {
        id: hoverTimer
        interval: 16   // ~60 Hz throttle
        repeat: false
        onTriggered: {
            if (_pendingMousePos.x < 0 || _pendingMousePos.y < 0)
                return;

            var dx = _pendingMousePos.x - _lastProcessedMousePos.x;
            var dy = _pendingMousePos.y - _lastProcessedMousePos.y;
            var min2 = hoverMinMovePx * hoverMinMovePx;

            if (!_forceHoverRecalc && (dx*dx + dy*dy) < min2)
                return;

            _forceHoverRecalc = false;
            _lastProcessedMousePos = _pendingMousePos;

            root._updateHoverAt(_pendingMousePos);
        }
    }

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

        // If the map moves/zooms while the mouse is stationary, re-evaluate hover.
        onZoomLevelChanged: if (mapHoverArea.containsMouse) root._pokeHoverRecalc()
        onCenterChanged:    if (mapHoverArea.containsMouse) root._pokeHoverRecalc()
        onBearingChanged:   if (mapHoverArea.containsMouse) root._pokeHoverRecalc()
        onTiltChanged:      if (mapHoverArea.containsMouse) root._pokeHoverRecalc()

        // Pan gesture via drag - keeps point under cursor stationary
        DragHandler {
            id: dragHandler
            target: null
            property var startCenter
            onActiveChanged: {
                if (active) {
                    startCenter = map.center
                }
            }
            onTranslationChanged: {
                if (!startCenter) return
                var startPt = map.fromCoordinate(startCenter, false)
                var newCenter = map.toCoordinate(Qt.point(startPt.x - translation.x,
                                                          startPt.y - translation.y))
                map.center = newCenter
            }
        }

        // Pinch-to-zoom gesture
        PinchHandler {
            id: pinchHandler
            target: null
            onScaleChanged: {
                var newZoom = map.zoomLevel + Math.log2(scale / activeScale)
                map.zoomLevel = Math.max(map.minimumZoomLevel,
                                Math.min(map.maximumZoomLevel, newZoom))
            }
            property real activeScale: 1.0
            onActiveChanged: if (active) activeScale = scale
        }

        // Hover and wheel zoom handler
        MouseArea {
            id: mapHoverArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton

            onPositionChanged: function(mouse) {
                // Ignore while dragging (mouse buttons down)
                if (mouse.buttons !== Qt.NoButton)
                    return;
                root._requestHoverUpdate(mouse.x, mouse.y)
            }

            onExited: {
                root._pendingMousePos = Qt.point(-1, -1)
                root._clearMapHover()
            }

            onWheel: function(wheel) {
                // Smooth zoom like Google Maps (~0.2 per scroll notch)
                // Keeps point under cursor stationary
                var zoomDelta = wheel.angleDelta.y / 600.0
                var newZoom = Math.max(map.minimumZoomLevel,
                              Math.min(map.maximumZoomLevel,
                                       map.zoomLevel + zoomDelta))
                if (newZoom === map.zoomLevel) return

                // Get coordinate under cursor before zoom
                var mouseCoord = map.toCoordinate(Qt.point(wheel.x, wheel.y))

                map.zoomLevel = newZoom

                // Get screen position of that coordinate after zoom
                var newMousePt = map.fromCoordinate(mouseCoord, false)

                // Shift center to keep cursor point stationary
                var dx = wheel.x - newMousePt.x
                var dy = wheel.y - newMousePt.y
                var centerPt = map.fromCoordinate(map.center, false)
                map.center = map.toCoordinate(Qt.point(centerPt.x - dx, centerPt.y - dy))
            }
        }

        // Tracks
        MapItemView {
            id: tracksView
            model: trackModel

            delegate: MapPolyline {
                id: poly
                z: 10
                line.width: mapPreferences ? mapPreferences.lineThickness : 3
                line.color: trackColor

                // Expect TrackMapModel roles:
                //   sessionId: string
                //   trackPoints: [{lat, lon, t}, ...]   (t is required for map hover)
                property string _sessionId: sessionId || ""
                property var _trackPoints: trackPoints || []

                Component.onCompleted: root._registerTrack(poly)
                Component.onDestruction: root._unregisterTrack(poly)

                path: _trackPoints
                      ? _trackPoints.map(function(p) { return QtPositioning.coordinate(p.lat, p.lon) })
                      : []
            }
        }

        // Cursor dots overlay (driven by CursorModel via MapCursorDotModel)
        MapItemView {
            id: cursorDotsView
            z: 20
            model: (typeof mapCursorDots !== "undefined") ? mapCursorDots : null

            delegate: MapQuickItem {
                id: dot
                z: 20

                // Expected dot roles: lat (double), lon (double), color (QColor)
                coordinate: QtPositioning.coordinate(lat, lon)

                // Avoid name clashes/binding loops by copying role -> different property name
                property color dotColor: (typeof color !== "undefined") ? color : "red"

                anchorPoint.x: dotItem.width / 2
                anchorPoint.y: dotItem.height / 2

                sourceItem: Rectangle {
                    id: dotItem
                    width: mapPreferences ? mapPreferences.markerSize : 10
                    height: mapPreferences ? mapPreferences.markerSize : 10
                    radius: width / 2
                    color: dot.dotColor
                    border.width: 2
                    border.color: "white"
                }
            }
        }
    }

    // Map type selector overlay
    Rectangle {
        id: mapTypeSelector
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 8
        z: 100
        width: selectedLabel.implicitWidth + 28
        height: 28
        radius: 4
        color: dropdownArea.containsMouse ? "#f0f0f0" : "#e0ffffff"
        border.color: "#aaaaaa"
        border.width: 1

        property int currentIndex: 0
        property var mapTypes: map.supportedMapTypes

        function applyPreference() {
            if (!mapPreferences) return
            var idx = mapPreferences.mapTypeIndex
            if (idx >= 0 && idx < mapTypes.length) {
                currentIndex = idx
                map.activeMapType = mapTypes[idx]
            }
        }

        function selectType(index) {
            if (index < 0 || index >= mapTypes.length) return
            currentIndex = index
            map.activeMapType = mapTypes[index]
            if (mapPreferences)
                mapPreferences.mapTypeIndex = index
            dropdownList.visible = false
        }

        Component.onCompleted: applyPreference()

        Connections {
            target: mapPreferences || null
            function onMapTypeIndexChanged() { mapTypeSelector.applyPreference() }
        }

        Text {
            id: selectedLabel
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: (mapTypeSelector.currentIndex >= 0
                   && mapTypeSelector.currentIndex < mapTypeSelector.mapTypes.length)
                  ? mapTypeSelector.mapTypes[mapTypeSelector.currentIndex].name
                  : ""
            font.pixelSize: 12
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: dropdownList.visible ? "\u25B2" : "\u25BC"
            font.pixelSize: 8
            color: "#666666"
        }

        MouseArea {
            id: dropdownArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: dropdownList.visible = !dropdownList.visible
        }

        // Dropdown list
        Rectangle {
            id: dropdownList
            visible: false
            anchors.top: parent.bottom
            anchors.topMargin: 2
            anchors.right: parent.right
            width: Math.max(parent.width, 160)
            height: dropdownColumn.implicitHeight + 4
            radius: 4
            color: "#f8f8f8"
            border.color: "#aaaaaa"
            border.width: 1

            Column {
                id: dropdownColumn
                anchors.fill: parent
                anchors.margins: 2

                Repeater {
                    model: mapTypeSelector.mapTypes

                    Rectangle {
                        width: dropdownColumn.width
                        height: 26
                        radius: 2
                        color: itemArea.containsMouse ? "#d0d0d0"
                             : (index === mapTypeSelector.currentIndex ? "#e0e0e0" : "transparent")

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.name
                            font.pixelSize: 12
                            font.bold: index === mapTypeSelector.currentIndex
                        }

                        MouseArea {
                            id: itemArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: mapTypeSelector.selectType(index)
                        }
                    }
                }
            }
        }
    }
}
