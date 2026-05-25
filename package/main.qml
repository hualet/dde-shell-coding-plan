// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0
import org.deepin.dtk 1.0

AppletItem {
    id: root
    objectName: "coding plan applet"

    readonly property bool useColumnLayout: Panel.position % 2
    readonly property int dockSize: Panel.rootObject.dockSize || 48
    readonly property var quotaSnapshots: Applet.quota ? Applet.quota.snapshots : []
    readonly property int visibleRingCount: Math.min(4, quotaSnapshots.length)
    property var selectedProvider: ({})
    property int dockOrder: 12

    implicitWidth: useColumnLayout ? dockSize : Math.max(dockSize, visibleRingCount * 24 + 16)
    implicitHeight: dockSize

    function severityColor(severity) {
        if (severity === "normal")
            return "#28c76f"
        if (severity === "warning")
            return "#f5a623"
        if (severity === "critical")
            return "#e5484d"
        return "#8b949e"
    }

    function providerInitial(name) {
        if (!name || name.length === 0)
            return "?"
        return name.charAt(0).toUpperCase()
    }

    function providerConfig(providerId) {
        const items = Applet.quota ? Applet.quota.providers : []
        for (let index = 0; index < items.length; ++index) {
            if (items[index].id === providerId)
                return items[index]
        }
        return {}
    }

    PanelToolTip {
        id: toolTip
        text: Applet.quota ? Applet.quota.tooltipText : ""
        toolTipX: DockPanelPositioner.x
        toolTipY: DockPanelPositioner.y
    }

    HoverHandler {
        onHoveredChanged: {
            if (hovered && !popup.popupVisible && toolTip.text.length > 0) {
                const point = root.mapToItem(null, root.width / 2, root.height / 2)
                toolTip.DockPanelPositioner.bounding = Qt.rect(point.x, point.y, toolTip.width, toolTip.height)
                toolTip.open()
            } else {
                toolTip.close()
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: popup.popupVisible = !popup.popupVisible
    }

    Row {
        anchors.centerIn: parent
        spacing: 4
        visible: root.visibleRingCount > 0

        Repeater {
            model: root.visibleRingCount

            Canvas {
                id: ring
                width: 20
                height: 20
                readonly property var snapshot: root.quotaSnapshots[index]

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const center = width / 2
                    const radius = width / 2 - 2
                    const ratio = Math.max(0, Math.min(1, snapshot.remainingRatio || 0))
                    ctx.lineWidth = 2.4
                    ctx.strokeStyle = "rgba(128, 128, 128, 0.24)"
                    ctx.beginPath()
                    ctx.arc(center, center, radius, 0, Math.PI * 2)
                    ctx.stroke()
                    ctx.strokeStyle = root.severityColor(snapshot.severity)
                    ctx.beginPath()
                    ctx.arc(center, center, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * ratio)
                    ctx.stroke()
                }

                Connections {
                    target: Applet.quota
                    function onSnapshotsChanged() {
                        ring.requestPaint()
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: root.providerInitial(parent.snapshot.providerName)
                    font.pixelSize: 9
                    font.bold: true
                    color: DockPalette.iconTextPalette.color
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.visibleRingCount === 0
        text: "+"
        font.pixelSize: 20
        color: DockPalette.iconTextPalette.color
    }

    PanelPopup {
        id: popup
        width: 390
        height: popupContent.implicitHeight
        popupX: DockPanelPositioner.x
        popupY: DockPanelPositioner.y

        Control {
            id: popupContent
            width: 390
            padding: 14

            contentItem: ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Coding Plan")
                        font.pixelSize: 17
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Refresh")
                        onClicked: Applet.quota.refreshAll()
                    }
                }

                Repeater {
                    model: root.quotaSnapshots

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 8
                        color: "transparent"
                        border.width: 1
                        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.22)
                        implicitHeight: cardColumn.implicitHeight + 18

                        ColumnLayout {
                            id: cardColumn
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: modelData.providerName
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: modelData.source
                                    opacity: 0.7
                                }
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                value: Math.max(0, modelData.remainingRatio)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: modelData.remainingRatio >= 0
                                      ? qsTr("%1 remaining").arg(Math.round(modelData.remainingRatio * 100) + "%")
                                      : modelData.message
                                wrapMode: Text.WordWrap
                                opacity: 0.85
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                Button {
                                    text: qsTr("Login")
                                    onClicked: {
                                        root.selectedProvider = modelData
                                        webPopup.popupVisible = true
                                    }
                                }

                                Button {
                                    text: qsTr("Console")
                                    onClicked: Applet.quota.openConsole(modelData.providerId)
                                }

                                Button {
                                    text: qsTr("Refresh")
                                    onClicked: Applet.quota.refreshProvider(modelData.providerId)
                                }

                                Button {
                                    text: qsTr("Manual 50%")
                                    onClicked: Applet.quota.setManualRatio(modelData.providerId, 0.5)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    PanelPopup {
        id: webPopup
        width: 900
        height: 640
        popupX: DockPanelPositioner.x
        popupY: DockPanelPositioner.y

        Loader {
            anchors.fill: parent
            active: webPopup.popupVisible
            source: "ProviderWebView.qml"

            onLoaded: {
                const provider = root.providerConfig(root.selectedProvider.providerId || "")
                item.providerId = root.selectedProvider.providerId || ""
                item.providerName = root.selectedProvider.providerName || ""
                item.loginUrl = provider.loginUrl || ""
                item.quotaUrl = provider.quotaUrl || ""
                item.allowedOrigins = provider.allowedOrigins || []
                item.extractorScript = provider.extractorScript || ""
                item.closeRequested.connect(function() {
                    webPopup.popupVisible = false
                })
                item.extracted.connect(function(remainingRatio) {
                    Applet.quota.setManualRatio(root.selectedProvider.providerId, remainingRatio)
                    webPopup.popupVisible = false
                })
                item.extractionFailed.connect(function(message) {
                    Applet.quota.setProviderError(root.selectedProvider.providerId, message)
                })
            }
        }
    }
}
