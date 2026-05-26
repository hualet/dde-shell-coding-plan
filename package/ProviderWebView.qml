// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWebEngine 1.10

Item {
    id: root

    property string providerId
    property string providerName
    property url loginUrl
    property url quotaUrl
    property var allowedOrigins: []
    property string extractorScript

    property bool _autoMode: false
    property int _autoPhase: 0

    signal closeRequested()
    signal extracted(var result)
    signal extractionFailed(string message)

    function isAllowedOrigin(pageUrl) {
        const origin = pageUrl.toString().match(/^https?:\/\/[^/]+/)
        if (!origin)
            return false
        for (let index = 0; index < allowedOrigins.length; ++index) {
            if (origin[0] === allowedOrigins[index])
                return true
        }
        return false
    }

    function startAutoExtract() {
        root._autoMode = true
        root._autoPhase = 1
        webView.url = root.loginUrl
    }

    function _onNavigationFinished(ok) {
        if (!ok || !root._autoMode) return
        const currentUrl = webView.url.toString()

        if (root._autoPhase === 1) {
            if (root.isAllowedOrigin(webView.url) && currentUrl.indexOf("/login") === -1) {
                root._autoPhase = 2
                webView.url = root.quotaUrl
            }
        } else if (root._autoPhase === 2) {
            if (root.isAllowedOrigin(webView.url)) {
                root._autoPhase = 3
                _runExtraction()
            }
        }
    }

    function _runExtraction() {
        if (!root.isAllowedOrigin(webView.url)) {
            root.extractionFailed(qsTr("This provider only allows quota reading on declared official domains."))
            root._autoMode = false
            return
        }

        webView.runJavaScript(root.extractorScript, function(result) {
            if (result && result.status === "ok" && (result.remainingRatio >= 0 || result.fiveHourRemainingRatio >= 0)) {
                root.extracted(result)
            } else {
                root.extractionFailed(result && result.message ? result.message : qsTr("Quota could not be read from this page."))
            }
            root._autoMode = false
        })
    }

    WebEngineProfile {
        id: webProfile
        storageName: "coding-plan-" + root.providerId
        offTheRecord: false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.providerName
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Button {
                text: qsTr("Login")
                onClicked: webView.url = root.loginUrl
            }

            Button {
                text: qsTr("Quota")
                onClicked: webView.url = root.quotaUrl
            }

            Button {
                text: qsTr("Read")
                onClicked: {
                    root._autoMode = false
                    root._runExtraction()
                }
            }

            Button {
                text: qsTr("Auto Refresh")
                onClicked: root.startAutoExtract()
            }

            Button {
                text: qsTr("Close")
                onClicked: root.closeRequested()
            }
        }

        WebEngineView {
            id: webView
            Layout.fillWidth: true
            Layout.fillHeight: true
            profile: webProfile
            url: root.loginUrl

            onLoadingChanged: {
                if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                    root._onNavigationFinished(true)
                } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                    root._onNavigationFinished(false)
                }
            }
        }
    }
}
