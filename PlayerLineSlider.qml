import QtQuick 2.0

Rectangle {
  id: root
  height: 4

  property var progress: 0.3
  property bool sellected: _mouse.containsMouse | _mouse.pressed

  color: "#404040"
  radius: 2

  Rectangle {
    id: _progress
    anchors.left: root.left
    height: root.height
    width: root.width * progress

    radius: root.radius
    color: sellected? "#FCE165" : "#AAAAAA"
  }

  Rectangle {
    id: _point
    visible: root.sellected
    width: 12
    height: 12
    radius: height / 2

    anchors.verticalCenter: root.verticalCenter
    x: root.width * progress - width / 2

    color: "#FFFFFF"
  }

  MouseArea {
    id: _mouse
    anchors.centerIn: root
    width: root.width + _point.width
    height: root.height + _point.height

    hoverEnabled: true

    onMouseXChanged: {
      if (pressed) {
        var progress = (mouseX - root.x - _point.width / 2) / root.width
        progress = Math.max(0, Math.min(1, progress))
        root.progress = progress
      }
    }
  }
}
