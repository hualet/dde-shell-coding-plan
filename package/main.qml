// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as LP
import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0
import org.deepin.dtk 1.0

AppletItem {
    id: root
    objectName: "coding plan applet"

    readonly property bool useColumnLayout: Panel.position % 2
    readonly property bool useClassicTaskbarLayout: Panel.itemAlignment === Dock.LeftAlignment
    readonly property int dockSize: Panel.rootObject.dockSize || 48
    readonly property var quotaSnapshots: Applet.quota ? Applet.quota.snapshots : []
    readonly property int visibleRingCount: Math.min(4, quotaSnapshots.length)
    property var selectedProvider: ({})
    property int dockOrder: useClassicTaskbarLayout ? 21 : 10
    property bool _reopenPopupAfterWebClose: false

    implicitWidth: useColumnLayout ? dockSize : Math.max(dockSize, visibleRingCount * (dockSize / 2) + (visibleRingCount - 1) * 4 + 16)
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

    function quotaPercent(snapshot) {
        const ratio = Math.max(0, Math.min(1, snapshot.remainingRatio || 0))
        return Math.round(ratio * 100) + "%"
    }

    function fiveHourQuotaPercent(snapshot) {
        const raw = snapshot.fiveHourRemainingRatio
        if (raw === undefined || raw === null || raw < 0) {
            const text = snapshot.fiveHourBalanceText || ""
            if (text.length > 0) return text
            return qsTr("N/A")
        }
        const ratio = Math.max(0, Math.min(1, raw))
        return Math.round(ratio * 100) + "%"
    }

    function openLoginCenter(provider) {
        if (provider && provider.providerId) {
            root.selectedProvider = provider
        } else if (!root.selectedProvider.providerId && root.quotaSnapshots.length > 0) {
            root.selectedProvider = root.quotaSnapshots[0]
        }

        popup.close()
        webPopup.open()
        root.configureWebView()
    }

    function configureWebView() {
        if (!webLoader.item || !root.selectedProvider.providerId)
            return

        const provider = root.providerConfig(root.selectedProvider.providerId)
        webLoader.item.providerId = root.selectedProvider.providerId
        webLoader.item.providerName = root.selectedProvider.providerName
        webLoader.item.loginUrl = provider.loginUrl || ""
        webLoader.item.quotaUrl = provider.quotaUrl || ""
        webLoader.item.allowedOrigins = provider.allowedOrigins || []
        webLoader.item.extractorScript = provider.extractorScript || ""
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

    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: {
            if (popup.popupVisible) {
                popup.close()
            } else {
                Panel.requestClosePopup()
                popup.open()
            }
            toolTip.close()
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        preventStealing: true

        onClicked: function(mouse) {
            popup.close()
            webPopup.close()
            settingsMenuLoader.active = true
            settingsMenuLoader.item.open()
            mouse.accepted = true
        }

        onPressed: function(mouse) {
            mouse.accepted = true
        }
    }

    Loader {
        id: settingsMenuLoader
        active: false
        sourceComponent: LP.Menu {
            LP.MenuItem {
                text: qsTr("设置")
                onTriggered: Applet.showSettings()
            }
        }
    }

    Row {
        anchors.centerIn: parent
        spacing: 4
        visible: root.visibleRingCount > 0

        Repeater {
            model: root.visibleRingCount

            Canvas {
                id: ring
                width: root.dockSize / 2
                height: root.dockSize / 2
                readonly property var snapshot: root.quotaSnapshots[index]

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const center = width / 2

                    // Outer ring - weekly quota
                    const outerRadius = center - 3
                    const outerRatio = Math.max(0, Math.min(1, snapshot.remainingRatio || 0))

                    // Inner ring - 5-hour quota
                    const innerRadius = center - 7
                    const innerRaw = snapshot.fiveHourRemainingRatio
                    const innerRatio = (innerRaw !== undefined && innerRaw !== null && innerRaw >= 0)
                        ? Math.max(0, Math.min(1, innerRaw))
                        : (snapshot.remainingRatio >= 0 ? Math.max(0, Math.min(1, snapshot.remainingRatio)) : 0)

                    // Background arcs
                    ctx.lineWidth = 3
                    ctx.strokeStyle = "rgba(128, 128, 128, 0.24)"
                    ctx.beginPath()
                    ctx.arc(center, center, outerRadius, 0, Math.PI * 2)
                    ctx.stroke()

                    ctx.lineWidth = 2.5
                    ctx.beginPath()
                    ctx.arc(center, center, innerRadius, 0, Math.PI * 2)
                    ctx.stroke()

                    // Colored arcs
                    const color = root.severityColor(snapshot.severity)

                    ctx.strokeStyle = color
                    ctx.lineWidth = 3
                    ctx.beginPath()
                    ctx.arc(center, center, outerRadius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * outerRatio)
                    ctx.stroke()

                    ctx.lineWidth = 2.5
                    ctx.beginPath()
                    ctx.arc(center, center, innerRadius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * innerRatio)
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
                    font.pixelSize: Math.max(9, parent.width * 0.35)
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
                                spacing: 7

                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28

                                    ProgressBar {
                                        anchors.fill: parent
                                        from: 0
                                        to: 1
                                        value: Math.max(0, modelData.fiveHourRemainingRatio >= 0 ? modelData.fiveHourRemainingRatio : modelData.remainingRatio)
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        text: qsTr("5小时额度：%1").arg(root.fiveHourQuotaPercent(modelData))
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: DockPalette.iconTextPalette.color
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28

                                    ProgressBar {
                                        anchors.fill: parent
                                        from: 0
                                        to: 1
                                        value: Math.max(0, modelData.remainingRatio)
                                    }

                                    Label {
                                        anchors.centerIn: parent
                                        text: qsTr("周额度：%1").arg(root.quotaPercent(modelData))
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: DockPalette.iconTextPalette.color
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

        onPopupVisibleChanged: {
            if (!webPopup.popupVisible && root._reopenPopupAfterWebClose) {
                root._reopenPopupAfterWebClose = false
                popup.open()
            }
        }

        Control {
            anchors.fill: parent
            padding: 12

            contentItem: RowLayout {
                spacing: 12

                ColumnLayout {
                    Layout.preferredWidth: 250
                    Layout.fillHeight: true
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Coding Plan")
                            font.pixelSize: 17
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Button {
                            text: qsTr("Close")
                            onClicked: webPopup.popupVisible = false
                        }
                    }

                    Repeater {
                        model: root.quotaSnapshots

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 8
                            color: modelData.providerId === root.selectedProvider.providerId
                                   ? Qt.rgba(0.2, 0.45, 0.9, 0.16)
                                   : "transparent"
                            border.width: 1
                            border.color: Qt.rgba(0.5, 0.5, 0.5, 0.22)
                            implicitHeight: providerCard.implicitHeight + 18

                            ColumnLayout {
                                id: providerCard
                                anchors.fill: parent
                                anchors.margins: 9
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: modelData.providerName
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: modelData.status === "ok" ? qsTr("Signed in")
                                             : modelData.status === "authenticated" ? qsTr("Authenticated")
                                             : modelData.status === "parse_error" ? qsTr("Parse failed")
                                             : qsTr("Login needed")
                                        color: root.severityColor(modelData.severity)
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.remainingRatio >= 0
                                          ? qsTr("%1 remaining").arg(Math.round(modelData.remainingRatio * 100) + "%")
                                          : modelData.message
                                    wrapMode: Text.WordWrap
                                    opacity: 0.8
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.selectedProvider = modelData
                                    root.configureWebView()
                                }
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Refresh All")
                        onClicked: Applet.quota.refreshAll()
                    }
                }

                Loader {
                    id: webLoader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    active: webPopup.popupVisible
                    source: "ProviderWebView.qml"

                    onLoaded: {
                        root.configureWebView()
                        item.closeRequested.connect(function() {
                            webPopup.popupVisible = false
                        })
                        item.extracted.connect(function(result) {
                            Applet.quota.setWebViewResult(root.selectedProvider.providerId, result)
                            if (item._wasAutoMode) {
                                autoCloseTimer.start()
                            }
                        })
                        item.extractionFailed.connect(function(message) {
                            Applet.quota.setProviderError(root.selectedProvider.providerId, message)
                            if (item._wasAutoMode) {
                                autoCloseTimer.start()
                            }
                        })
                        item.loginSucceeded.connect(function(providerId) {
                            Applet.quota.setProviderAuthenticated(providerId)
                        })
                        if (root.selectedProvider.providerId) {
                            item.startAutoExtract()
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: autoCloseTimer
        interval: 800
        repeat: false
        onTriggered: {
            root._reopenPopupAfterWebClose = true
            webPopup.popupVisible = false
        }
    }
}
