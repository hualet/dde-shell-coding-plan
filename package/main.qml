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
    readonly property bool useClassicTaskbarLayout: Panel.itemAlignment === Dock.LeftAlignment
    readonly property int dockSize: Panel.rootObject.dockSize || 48
    readonly property var quotaSnapshots: Applet.quota ? Applet.quota.snapshots : []
    readonly property int visibleRingCount: Math.min(4, quotaSnapshots.length)

    property int dockOrder: useClassicTaskbarLayout ? 21 : 10

    property Palette secondaryTextColor: Palette {
        normal: Qt.rgba(0, 0, 0, 0.5)
        normalDark: Qt.rgba(1, 1, 1, 0.5)
    }

    implicitWidth: useColumnLayout ? dockSize : Math.max(dockSize, visibleRingCount * (dockSize * 3 / 5) + (visibleRingCount - 1) * 4 + 16)
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

    function fiveHourSeverity(snapshot) {
        const raw = snapshot ? snapshot.fiveHourRemainingRatio : -1
        if (raw === undefined || raw === null || raw < 0)
            return "error"
        if (raw < 0.10)
            return "critical"
        if (raw <= 0.30)
            return "warning"
        return "normal"
    }

    function refreshAll() {
        if (Applet.quota) {
            Applet.quota.refreshAll()
        }
    }

    function copyToClipboard(text) {
        clipboardHelper.text = text
        clipboardHelper.selectAll()
        clipboardHelper.copy()
    }

    TextEdit {
        id: clipboardHelper
        visible: false
    }

    Timer {
        id: copyTimer
        interval: 1500
        property string tokenText: ""
        onTriggered: tokenButton.text = qsTr("复制配对 Token")
    }

    PanelToolTip {
        id: toolTip
        text: Applet.quota ? Applet.quota.tooltipText : ""
        toolTipX: DockPanelPositioner.x
        toolTipY: DockPanelPositioner.y
    }

    Connections {
        target: Applet.quota
        function onBackgroundRefreshRequested() {
            root.refreshAll()
        }
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

    Row {
        anchors.centerIn: parent
        spacing: 4
        visible: root.visibleRingCount > 0

        Repeater {
            model: root.visibleRingCount

            Canvas {
                id: ring
                width: root.dockSize * 3 / 5
                height: root.dockSize * 3 / 5
                readonly property var snapshot: root.quotaSnapshots[index]

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const center = width / 2
                    const lineWidth = 6
                    const radius = center - lineWidth / 2
                    const raw = snapshot.fiveHourRemainingRatio
                    const ratio = (raw !== undefined && raw !== null && raw >= 0)
                        ? Math.max(0, Math.min(1, raw))
                        : 0

                    ctx.lineWidth = lineWidth
                    ctx.strokeStyle = "rgba(128, 128, 128, 0.24)"
                    ctx.beginPath()
                    ctx.arc(center, center, radius, 0, Math.PI * 2)
                    ctx.stroke()

                    ctx.strokeStyle = root.severityColor(root.fiveHourSeverity(snapshot))
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

        Component.onCompleted: {
            DockPanelPositioner.bounding = Qt.binding(function () {
                const point = root.mapToItem(null, root.width / 2, root.height / 2)
                return Qt.rect(point.x, point.y, popup.width, popup.height)
            })
        }

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

                    Label {
                        visible: Applet.quota && Applet.quota.extensionConnected
                        text: qsTr("已连接")
                        color: "#28c76f"
                        font.pixelSize: 12
                    }

                    Label {
                        visible: !Applet.quota || !Applet.quota.extensionConnected
                        text: qsTr("未连接")
                        color: root.secondaryTextColor.color
                        font.pixelSize: 12
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

                                RowLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: modelData.providerName
                                        font.pixelSize: 14
                                        font.bold: true
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    Item {
                                        implicitWidth: 28
                                        implicitHeight: 28

                                        DciIcon {
                                            anchors.centerIn: parent
                                            name: "utilities-terminal-symbolic"
                                            sourceSize: Qt.size(16, 16)
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (Applet.quota)
                                                    Applet.quota.openConsole(modelData.providerId)
                                            }
                                        }
                                    }
                                }

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
                                    }
                                }

                            }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Button {
                        text: qsTr("刷新全部")
                        Layout.fillWidth: true
                        onClicked: root.refreshAll()
                    }

                    Button {
                        id: tokenButton
                        text: qsTr("复制配对 Token")
                        Layout.fillWidth: true
                        onClicked: {
                            if (Applet.quota) {
                                var tok = Applet.quota.extensionToken || ""
                                if (tok.length > 0) {
                                    root.copyToClipboard(tok)
                                    text = qsTr("已复制")
                                    copyTimer.start()
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: Applet.quota && !Applet.quota.extensionConnected
                    text: qsTr("请安装浏览器扩展并输入配对 Token 以连接。")
                    wrapMode: Text.WordWrap
                    color: root.secondaryTextColor.color
                    font.pixelSize: 12
                }
            }
        }
    }

}
