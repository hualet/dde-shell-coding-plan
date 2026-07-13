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
    readonly property bool isDarkTheme: DTK.themeType === ApplicationHelper.DarkType

    property int dockOrder: useClassicTaskbarLayout ? 21 : 10

    // Plain JS colors (DTK Palette.normal/normalDark are group objects, not colors).
    readonly property color secondaryTextColor: root.isDarkTheme ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(0, 0, 0, 0.5)
    readonly property color ringTextColor: root.isDarkTheme ? Qt.rgba(1, 1, 1, 0.95) : Qt.rgba(0, 0, 0, 0.9)
    readonly property color ringTrackColor: root.isDarkTheme ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color errorRingColor: "#ef4444"

    readonly property real ringSize: dockSize * 3 / 5
    readonly property real ringsExtent: visibleRingCount * ringSize + (visibleRingCount - 1) * 4 + 16

    implicitWidth: useColumnLayout ? dockSize : Math.max(dockSize, ringsExtent)
    implicitHeight: useColumnLayout ? Math.max(dockSize, ringsExtent) : dockSize

    function severityColor(severity) {
        if (severity === "normal")
            return "#28c76f"
        if (severity === "warning")
            return "#f5a623"
        if (severity === "critical")
            return "#e5484d"
        return "#8b949e"
    }

    // Inner ring uses a slightly lighter shade of each severity color so the
    // two rings are distinguishable when both are visible.
    function severityColorInner(severity) {
        if (severity === "normal")
            return "#5dd896"
        if (severity === "warning")
            return "#fac06a"
        if (severity === "critical")
            return "#f07a80"
        return "#aeb6c2"
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

    // In horizontal dock mode the rings sit side by side; in vertical (left/
    // right) dock mode they stack so they don't overflow the narrow column.
    Repeater {
        model: root.visibleRingCount

        Canvas {
            id: ring
            width: root.ringSize
            height: root.ringSize
            // Stack vertically in column layout, lay out horizontally otherwise.
            x: root.useColumnLayout
                ? (parent.width - width) / 2
                : (parent.width - root.ringsExtent) / 2 + index * (root.ringSize + 4)
            y: root.useColumnLayout
                ? (parent.height - root.ringsExtent) / 2 + index * (root.ringSize + 4)
                : (parent.height - height) / 2
            readonly property var snapshot: root.quotaSnapshots[index]

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const center = width / 2
                const ringWidth = 4
                const ringGap = 1
                const outerRadius = center - ringWidth / 2
                const innerRadius = center - ringWidth - ringGap - ringWidth / 2
                const isError = snapshot.severity === "error"

                const weeklyRaw = snapshot.remainingRatio
                const weeklyRatio = (weeklyRaw !== undefined && weeklyRaw !== null && weeklyRaw >= 0)
                    ? Math.max(0, Math.min(1, weeklyRaw))
                    : 0

                const fiveHourRaw = snapshot.fiveHourRemainingRatio
                const fiveHourValid = (fiveHourRaw !== undefined && fiveHourRaw !== null && fiveHourRaw >= 0)
                const fiveHourRatio = fiveHourValid ? Math.max(0, Math.min(1, fiveHourRaw)) : 0

                function clampRaw(v) {
                    return (v !== undefined && v !== null && v >= 0) ? Math.max(0, Math.min(1, v)) : -1
                }

                ctx.lineWidth = ringWidth
                ctx.strokeStyle = root.ringTrackColor
                ctx.beginPath()
                ctx.arc(center, center, outerRadius, 0, Math.PI * 2)
                ctx.stroke()

                ctx.lineWidth = ringWidth
                ctx.strokeStyle = root.ringTrackColor
                ctx.beginPath()
                ctx.arc(center, center, innerRadius, 0, Math.PI * 2)
                ctx.stroke()

                if (isError) {
                    ctx.lineWidth = ringWidth
                    ctx.strokeStyle = root.errorRingColor
                    ctx.beginPath()
                    ctx.arc(center, center, outerRadius, 0, Math.PI * 2)
                    ctx.stroke()

                    ctx.lineWidth = ringWidth
                    ctx.strokeStyle = root.errorRingColor
                    ctx.beginPath()
                    ctx.arc(center, center, innerRadius, 0, Math.PI * 2)
                    ctx.stroke()
                } else {
                    const outerSeverity = clampRaw(weeklyRaw) < 0
                        ? "error"
                        : (clampRaw(weeklyRaw) < 0.10 ? "critical"
                            : (clampRaw(weeklyRaw) <= 0.30 ? "warning" : "normal"))
                    ctx.lineWidth = ringWidth
                    ctx.strokeStyle = root.severityColor(outerSeverity)
                    ctx.beginPath()
                    ctx.arc(center, center, outerRadius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * weeklyRatio)
                    ctx.stroke()

                    if (fiveHourValid) {
                        ctx.lineWidth = ringWidth
                        ctx.strokeStyle = root.severityColorInner(root.fiveHourSeverity(snapshot))
                        ctx.beginPath()
                        ctx.arc(center, center, innerRadius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * fiveHourRatio)
                        ctx.stroke()
                    }
                }
            }

            Connections {
                target: Applet.quota
                function onSnapshotsChanged() {
                    ring.requestPaint()
                }
            }

            Connections {
                target: root
                function onIsDarkThemeChanged() {
                    ring.requestPaint()
                }
            }

            Text {
                anchors.centerIn: parent
                text: root.providerInitial(parent.snapshot.providerName)
                font.pixelSize: Math.max(9, parent.width * 0.35)
                font.bold: true
                color: root.ringTextColor
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.visibleRingCount === 0
        text: "+"
        font.pixelSize: 20
        color: root.ringTextColor
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
                        color: root.secondaryTextColor
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
                                            name: "utilities-terminal"
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
                                        value: Math.max(0, modelData.fiveHourRemainingRatio >= 0 ? modelData.fiveHourRemainingRatio : 0)
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
                    color: root.secondaryTextColor
                    font.pixelSize: 12
                }
            }
        }
    }

}
