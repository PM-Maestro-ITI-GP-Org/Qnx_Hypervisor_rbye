import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    title: "Qt Quick 2D Shapes & Text Demo"

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0f0c29" }
            GradientStop { position: 0.5; color: "#302b63" }
            GradientStop { position: 1.0; color: "#24243e" }
        }

        // Particle starfield
        Canvas {
            anchors.fill: parent
            property var stars: []
            property int numStars: 120

            Component.onCompleted: {
                for (var i = 0; i < numStars; i++) {
                    stars.push({
                        x: Math.random() * width,
                        y: Math.random() * height,
                        r: Math.random() * 2 + 0.5,
                        speed: Math.random() * 0.5 + 0.1,
                        alpha: Math.random() * 0.8 + 0.2
                    })
                }
            }

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "#ffffff"
                for (var i = 0; i < stars.length; i++) {
                    var s = stars[i]
                    ctx.globalAlpha = s.alpha
                    ctx.beginPath()
                    ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            Timer {
                interval: 50
                running: true
                repeat: true
                onTriggered: {
                    for (var i = 0; i < parent.stars.length; i++) {
                        var s = parent.stars[i]
                        s.y += s.speed
                        if (s.y > parent.height) {
                            s.y = 0
                            s.x = Math.random() * parent.width
                        }
                        s.alpha = s.alpha + (Math.random() - 0.5) * 0.1
                        s.alpha = Math.max(0.1, Math.min(1.0, s.alpha))
                    }
                    parent.requestPaint()
                }
            }
        }

        // 2D Text Display Area
        Column {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 20
            spacing: 12
            z: 10

            Text {
                text: "2D Animation Demo"
                color: "#ffffff"
                font.pixelSize: 28
                font.bold: true
                style: Text.Outline
                styleColor: "#000000"
            }

            Text {
                text: "Bouncing Circle | Rotating Square | Pulsing Triangle"
                color: "#00ff88"
                font.pixelSize: 16
                font.family: "Courier"
                style: Text.Outline
                styleColor: "#000000"
            }

            Text {
                id: infoText
                text: "Circle Y: " + Math.round(circle.y) + "  Square Rotation: " + Math.round(square.rotation) + "°  Triangle Scale: " + triangleCanvas.tScale.toFixed(2)
                color: "#ffaa00"
                font.pixelSize: 14
            }

            Text {
                id: bounceText
                text: "Bounce Count: 0"
                color: "#00aaff"
                font.pixelSize: 14
            }
        }

        // Animated Circle (bouncing)
        Rectangle {
            id: circle
            x: 100
            y: 100
            width: 60
            height: 60
            radius: 30
            color: "#ff6b6b"
            opacity: 0.9

            Rectangle {
                anchors.centerIn: parent
                width: 20
                height: 20
                radius: 10
                color: "#ffaaaa"
                opacity: 0.5
            }

            property real dx: 3
            property real dy: 2
            property int bounceCount: 0

            Timer {
                interval: 16
                running: true
                repeat: true
                onTriggered: {
                    var r = parent
                    r.x += r.dx
                    r.y += r.dy

                    if (r.x + r.width > root.width || r.x < 0) {
                        r.dx = -r.dx
                        r.bounceCount++
                        r.color = Qt.hsla(Math.random(), 0.7, 0.6, 1.0)
                    }
                    if (r.y + r.height > root.height || r.y < 0) {
                        r.dy = -r.dy
                        r.bounceCount++
                        r.color = Qt.hsla(Math.random(), 0.7, 0.6, 1.0)
                    }

                    bounceText.text = "Bounce Count: " + r.bounceCount
                    infoText.text = "Circle Y: " + Math.round(r.y) + "  Square Rotation: " + Math.round(square.rotation) + "°  Triangle Scale: " + triangleCanvas.tScale.toFixed(2)
                }
            }
        }

        // Rotating Square
        Rectangle {
            id: square
            x: 400
            y: 250
            width: 80
            height: 80
            color: "#4ecdc4"
            opacity: 0.85
            border.color: "#ffffff"
            border.width: 2

            SequentialAnimation on rotation {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 0; to: 360; duration: 4000; easing.type: Easing.InOutQuad }
            }

            SequentialAnimation on x {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 350; to: 550; duration: 3000; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 550; to: 350; duration: 3000; easing.type: Easing.InOutQuad }
            }

            SequentialAnimation on y {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 200; to: 400; duration: 2500; easing.type: Easing.InOutSine }
                NumberAnimation { from: 400; to: 200; duration: 2500; easing.type: Easing.InOutSine }
            }

            Text {
                anchors.centerIn: parent
                text: "Qt"
                color: "#ffffff"
                font.pixelSize: 24
                font.bold: true
            }
        }

        // Pulsing Triangle (Canvas drawn)
        Canvas {
            id: triangleCanvas
            x: 700
            y: 300
            width: 80
            height: 80
            opacity: 0.9

            property real tScale: 1.0

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.save()
                ctx.translate(width / 2, height / 2)
                ctx.scale(tScale, tScale)

                ctx.fillStyle = "#95e1d3"
                ctx.strokeStyle = "#ffffff"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(0, -30)
                ctx.lineTo(26, 20)
                ctx.lineTo(-26, 20)
                ctx.closePath()
                ctx.fill()
                ctx.stroke()

                ctx.restore()
            }

            SequentialAnimation on tScale {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 0.8; to: 1.5; duration: 2000; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 1.5; to: 0.8; duration: 2000; easing.type: Easing.InOutQuad }
            }

            SequentialAnimation on x {
                running: true
                loops: Animation.Infinite
                NumberAnimation { from: 650; to: 850; duration: 3500; easing.type: Easing.InOutCubic }
                NumberAnimation { from: 850; to: 650; duration: 3500; easing.type: Easing.InOutCubic }
            }

            onTScaleChanged: {
                requestPaint()
            }
        }

        // Bottom Controls
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 80
            color: "#1a1a2e"
            border.color: "#444444"
            border.width: 1
            z: 10

            Row {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                TextInput {
                    id: textInput
                    width: parent.width - 120
                    height: parent.height - 20
                    color: "#ffffff"
                    selectionColor: "#00ff00"
                    font.pixelSize: 14
                    text: "Enter text here..."
                    verticalAlignment: TextInput.AlignVCenter

                    Keys.onReturnPressed: {
                        displayText.text = textInput.text
                    }
                }

                Button {
                    width: 100
                    height: parent.height - 20
                    text: "Display"
                    onClicked: {
                        displayText.text = textInput.text
                    }
                }
            }
        }

        // Display text (shows up in the center)
        Text {
            id: displayText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: 32
            font.bold: true
            opacity: 0.3
            style: Text.Outline
            styleColor: "#000000"
            z: 5
        }
    }
}
