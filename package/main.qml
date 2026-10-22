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
    // Plans turned off in the settings dialog ("显示 Plan") leave the
    // taskbar; the popup follows the same filter.
    readonly property var taskbarSnapshots: quotaSnapshots.filter(function (s) { return s.visible !== false })
    readonly property int visibleRingCount: taskbarSnapshots.length
    readonly property bool isDarkTheme: DTK.themeType === ApplicationHelper.DarkType

    property int dockOrder: useClassicTaskbarLayout ? 21 : 10

    // Plain JS colors (DTK Palette.normal/normalDark are group objects, not colors).
    readonly property color secondaryTextColor: root.isDarkTheme ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(0, 0, 0, 0.5)
    readonly property color ringTextColor: root.isDarkTheme ? Qt.rgba(1, 1, 1, 0.95) : Qt.rgba(0, 0, 0, 0.9)
    readonly property color ringTrackColor: root.isDarkTheme ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color errorRingColor: "#ef4444"
    // Some sources answer instantly; keep the busy state visible briefly so
    // a click always shows that it did something.
    property bool refreshHold: false
    readonly property bool refreshBusy: refreshHold || (Applet.quota ? Applet.quota.refreshing : false)

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

    // Ring letters that would collide by name: Claude ("C" like Codex) is
    // shown by its maker's initial.
    readonly property var providerInitials: ({ "claude": "A" })

    function providerInitial(snapshot) {
        if (snapshot && root.providerInitials[snapshot.providerId])
            return root.providerInitials[snapshot.providerId]
        const name = snapshot ? snapshot.providerName : ""
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
            root.refreshHold = true
            refreshHoldTimer.restart()
            Applet.quota.refreshAll()
        }
    }

    Timer {
        id: refreshHoldTimer
        interval: 600
        onTriggered: root.refreshHold = false
    }

    function formatTime(value, withSeconds) {
        if (!value)
            return ""
        const date = new Date(value)
        if (isNaN(date.getTime()))
            return ""
        const time = withSeconds ? "HH:mm:ss" : "HH:mm"
        return date.toDateString() === new Date().toDateString()
            ? Qt.formatDateTime(date, time)
            : Qt.formatDateTime(date, "MM-dd " + time)
    }

    function isErrorStatus(snapshot) {
        return ["auth_error", "rate_limited", "unsupported", "parse_error", "network_error"]
            .indexOf(snapshot.status) >= 0
    }

    // Cards stay lean: plan name plus update time; entries that cannot be
    // refreshed show a one-line "Cannot update" reason instead.
    function cardStatusText(snapshot) {
        if (snapshot.refreshing)
            return qsTr("Refreshing…")
        if (root.isErrorStatus(snapshot)) {
            const reason = snapshot.message && snapshot.message.length > 0
                ? snapshot.message
                : qsTr("Failed to read")
            return qsTr("Cannot update: %1").arg(reason)
        }
        const updated = root.formatTime(snapshot.updatedAt, false)
        return updated.length > 0 ? qsTr("Updated at %1").arg(updated) : ""
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

    // Main settings entry: right-click the taskbar item.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        preventStealing: true

        onClicked: function (mouse) {
            if (popup.popupVisible)
                popup.close()
            toolTip.close()
            settingsMenuLoader.active = true
            settingsMenuLoader.item.open()
            mouse.accepted = true
        }

        onPressed: function (mouse) {
            mouse.accepted = true
        }
    }

    Loader {
        id: settingsMenuLoader
        active: false

        sourceComponent: LP.Menu {
            LP.MenuItem {
                text: qsTr("Settings")

                onTriggered: {
                    if (Applet.showSettings)
                        Applet.showSettings()
                }
            }
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
            readonly property var snapshot: root.taskbarSnapshots[index]

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
                text: root.providerInitial(parent.snapshot)
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

                    // Icon-only secondary entry; right-click is the main one.
                    Item {
                        implicitWidth: 30
                        implicitHeight: 30

                        DciIcon {
                            anchors.centerIn: parent
                            name: "preferences-system"
                            sourceSize: Qt.size(16, 16)
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (Applet.showSettings)
                                    Applet.showSettings()
                            }
                        }
                    }
                }

                Repeater {
                    model: root.taskbarSnapshots

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
                                        text: modelData.label && modelData.label.length > 0
                                            ? modelData.providerName + " · " + modelData.label
                                            : modelData.providerName
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

                                    Item {
                                        visible: modelData.source === "manual"
                                        implicitWidth: 22
                                        implicitHeight: 28

                                        Text {
                                            anchors.centerIn: parent
                                            text: "✕"
                                            font.pixelSize: 12
                                            color: root.secondaryTextColor
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (Applet.quota)
                                                    Applet.quota.removeAccount(modelData.entryId)
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
                                        text: qsTr("5-hour quota: %1").arg(root.fiveHourQuotaPercent(modelData))
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
                                        text: qsTr("Weekly quota: %1").arg(root.quotaPercent(modelData))
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                    text: root.cardStatusText(modelData)
                                    font.pixelSize: 11
                                    color: root.isErrorStatus(modelData) && !modelData.refreshing
                                        ? root.errorRingColor
                                        : root.secondaryTextColor
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                }

                            }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: root.refreshBusy ? qsTr("Refreshing…") : qsTr("Refresh All")
                    enabled: !root.refreshBusy
                    onClicked: root.refreshAll()
                }

                Label {
                    Layout.fillWidth: true
                    readonly property string lastRefresh: Applet.quota
                        ? root.formatTime(Applet.quota.lastRefreshAt, true) : ""
                    visible: lastRefresh.length > 0 && !root.refreshBusy
                    text: qsTr("Last refresh %1").arg(lastRefresh)
                    font.pixelSize: 11
                    color: root.secondaryTextColor
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.quotaSnapshots.length === 0
                    text: qsTr("No logged-in coding CLI detected (Codex / Claude / Kimi / GLM). Add an account in Settings.")
                    wrapMode: Text.WordWrap
                    color: root.secondaryTextColor
                    font.pixelSize: 12
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.quotaSnapshots.length > 0 && root.taskbarSnapshots.length === 0
                    text: qsTr("All plans are switched off. Turn them on in Settings.")
                    wrapMode: Text.WordWrap
                    color: root.secondaryTextColor
                    font.pixelSize: 12
                }
            }
        }
    }

}
