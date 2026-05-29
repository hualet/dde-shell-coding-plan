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
    property string _preLoginUrl: ""
    property bool _wasAutoMode: false
    readonly property bool wasAutoMode: _wasAutoMode
    property bool _loginSucceeded: false
    property int _extractionAttempts: 0
    property int _maxExtractionAttempts: 6

    signal closeRequested()
    signal extracted(var result)
    signal extractionFailed(string message)
    signal loginSucceeded(string providerId)

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

    function _urlBasePath(url) {
        var schemeEnd = url.indexOf("://")
        if (schemeEnd < 0) return url
        var pathStart = url.indexOf("/", schemeEnd + 3)
        if (pathStart < 0) return url
        var end = url.length
        var q = url.indexOf("?", pathStart)
        var h = url.indexOf("#", pathStart)
        if (q >= 0) end = Math.min(end, q)
        if (h >= 0) end = Math.min(end, h)
        return url.substring(0, end)
    }

    function _urlPath(url) {
        var str = url.toString()
        var schemeEnd = str.indexOf("://")
        if (schemeEnd < 0) return str
        var pathStart = str.indexOf("/", schemeEnd + 3)
        if (pathStart < 0) return "/"
        var end = str.length
        var q = str.indexOf("?", pathStart)
        var h = str.indexOf("#", pathStart)
        if (q >= 0) end = Math.min(end, q)
        if (h >= 0) end = Math.min(end, h)
        return str.substring(pathStart, end)
    }

    function _isOnLoginPage(url) {
        var urlStr = url.toString()
        if (urlStr.indexOf("/login") >= 0)
            return true
        var loginBase = root._urlBasePath(root.loginUrl.toString())
        var currentBase = root._urlBasePath(urlStr)
        return currentBase === loginBase
    }

    function _isOnQuotaPage(url) {
        var urlStr = url.toString()
        var quotaBase = root._urlBasePath(root.quotaUrl.toString())
        var currentBase = root._urlBasePath(urlStr)
        if (currentBase === quotaBase) {
            var loginPath = root._urlPath(root.loginUrl.toString())
            var quotaPath = root._urlPath(root.quotaUrl.toString())
            if (loginPath !== quotaPath) {
                var currentPath = root._urlPath(urlStr)
                if (currentPath === quotaPath)
                    return true
                return false
            }
            return true
        }
        return false
    }

    function startAutoExtract() {
        if (!root.webProfileConfigured) return
        root._autoMode = true
        root._autoPhase = 1
        root._extractionAttempts = 0
        root._preLoginUrl = webView.url.toString()
        webView.url = root.loginUrl
    }

    function _checkLoginSuccess() {
        if (!root._autoMode || root._autoPhase !== 1) return

        if (root._isOnQuotaPage(webView.url)) {
            if (!root._loginSucceeded) {
                root._loginSucceeded = true
                root.loginSucceeded(root.providerId)
            }
            root._autoPhase = 2
            root._runExtraction()
            return
        }

        if (!root._isOnLoginPage(webView.url) && root.isAllowedOrigin(webView.url)) {
            if (!root._loginSucceeded) {
                root._loginSucceeded = true
                root.loginSucceeded(root.providerId)
            }
            root._autoPhase = 2
            webView.url = root.quotaUrl
            return
        }

        loginCheckTimer.start()
    }

    function _onNavigationFinished(ok) {
        if (!ok || !root._autoMode) return

        if (loginCheckTimer._probingConsole) return

        if (root._autoPhase === 1) {
            if (root._isOnQuotaPage(webView.url)) {
                if (!root._loginSucceeded) {
                    root._loginSucceeded = true
                    root.loginSucceeded(root.providerId)
                }
                root._autoPhase = 2
                root._runExtraction()
                return
            }

            if (!root._isOnLoginPage(webView.url) && root.isAllowedOrigin(webView.url)) {
                if (!root._loginSucceeded) {
                    root._loginSucceeded = true
                    root.loginSucceeded(root.providerId)
                }
                root._autoPhase = 2
                webView.url = root.quotaUrl
                return
            }

            loginCheckTimer.start()
        } else if (root._autoPhase === 2) {
            if (root.isAllowedOrigin(webView.url)) {
                root._autoPhase = 3
                _runExtraction()
            }
        }
    }

    function _runExtraction() {
        if (!root.isAllowedOrigin(webView.url)) {
            console.warn("[coding-plan][webview]", root.providerId, "blocked origin", webView.url)
            root.extractionFailed(qsTr("This provider only allows quota reading on declared official domains."))
            root._autoMode = false
            return
        }

        root._extractionAttempts += 1
        console.warn("[coding-plan][webview]", root.providerId,
                    "extract attempt", root._extractionAttempts,
                    "url", webView.url)
        webView.runJavaScript(root.extractorScript, function(result) {
            if (!result || typeof result !== "object") {
                console.warn("[coding-plan][webview]", root.providerId,
                            "extract null-result", "attempt", root._extractionAttempts)
                if (root._autoMode && !root._loginSucceeded) {
                    root._loginSucceeded = true
                    root.loginSucceeded(root.providerId)
                }
                root._scheduleExtractionRetry(qsTr("Quota could not be read from this page."))
                return
            }

            if (root._autoMode && !root._loginSucceeded) {
                root._loginSucceeded = true
                root.loginSucceeded(root.providerId)
            }

            var hasRatio = (typeof result.remainingRatio === "number" && result.remainingRatio >= 0)
                         || (typeof result.fiveHourRemainingRatio === "number" && result.fiveHourRemainingRatio >= 0)
            var hasText = (typeof result.balanceText === "string" && result.balanceText.length > 0)
                       || (typeof result.fiveHourBalanceText === "string" && result.fiveHourBalanceText.length > 0)
            console.warn("[coding-plan][webview]", root.providerId,
                        "extract result", "attempt", root._extractionAttempts,
                        "status", result.status,
                        "remaining", result.remainingRatio,
                        "fiveHour", result.fiveHourRemainingRatio,
                        "balanceText", result.balanceText || "",
                        "fiveHourText", result.fiveHourBalanceText || "",
                        "message", result.message || "")
            root._wasAutoMode = root._autoMode
            if (result.status === "ok" && (hasRatio || hasText)) {
                root._extractionAttempts = 0
                root.extracted(result)
                root._autoMode = false
            } else {
                root._scheduleExtractionRetry(result.message ? result.message : qsTr("Quota could not be read from this page."))
            }
        })
    }

    function _scheduleExtractionRetry(message) {
        if (root._extractionAttempts < root._maxExtractionAttempts) {
            console.warn("[coding-plan][webview]", root.providerId,
                        "retry scheduled", root._extractionAttempts,
                        "message", message)
            extractionRetryTimer._lastMessage = message
            extractionRetryTimer.restart()
            return
        }

        root._finishExtractionFailure(message)
    }

    function _finishExtractionFailure(message) {
        console.warn("[coding-plan][webview]", root.providerId,
                    "extract failed", "attempts", root._extractionAttempts,
                    "message", message)
        root.extractionFailed(message)
        root._wasAutoMode = root._autoMode
        root._autoMode = false
        root._extractionAttempts = 0
    }

    function _isExpectedConsoleNoise(message) {
        return message.indexOf("DialogContent` requires a `DialogTitle") >= 0
            || message.indexOf("Unrecognized feature: 'ch-ua-form-factors'") >= 0
            || message.indexOf("Found a 'popover' attribute with an invalid value") >= 0
            || message.indexOf("RequestError: Failed to fetch") >= 0
            || message.indexOf("Failed to fetch") >= 0
            || (message.indexOf("DOMException") >= 0 && message.indexOf("[object Object]") >= 0)
            || message.indexOf("RecoverableError: Minified React error #418") >= 0
    }

    Timer {
        id: loginCheckTimer
        interval: 2500
        repeat: false
        property bool _probingConsole: false
        onTriggered: {
            if (!root._autoMode || root._autoPhase !== 1) return

            if (!_probingConsole) {
                _probingConsole = true
                webView.url = root.quotaUrl
                loginCheckTimer.start()
            } else {
                if (root.isAllowedOrigin(webView.url)) {
                    if (!root._loginSucceeded) {
                        root._loginSucceeded = true
                        root.loginSucceeded(root.providerId)
                    }
                    webView.runJavaScript(root.extractorScript, function(result) {
                        if (!root._autoMode || root._autoPhase !== 1) return
                        if (result && typeof result === "object" && result.status === "ok") {
                            _probingConsole = false
                            root._autoPhase = 2
                            root._runExtraction()
                        } else {
                            _probingConsole = false
                            webView.url = root.loginUrl
                            loginCheckTimer.start()
                        }
                    })
                } else {
                    _probingConsole = false
                    webView.url = root.loginUrl
                    loginCheckTimer.start()
                }
            }
        }
    }

    Timer {
        id: extractionRetryTimer
        interval: 1200
        repeat: false
        property string _lastMessage: ""
        onTriggered: root._runExtraction()
    }

    onProviderIdChanged: {
        if (root.providerId.length > 0 && !webProfileConfigured) {
            webProfile.storageName = "coding-plan-" + root.providerId
            webProfile.offTheRecord = false
            webProfile.persistentCookiesPolicy = WebEngineProfile.ForcePersistentCookies
            webProfileConfigured = true
            webView.profile = webProfile
            webView.url = root.loginUrl
        }
    }

    property bool webProfileConfigured: false

    WebEngineProfile {
        id: webProfile
        offTheRecord: false
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
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
                    root._extractionAttempts = 0
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

            onLoadingChanged: {
                if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                    root._onNavigationFinished(true)
                } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                    root._onNavigationFinished(false)
                }
            }

            onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceID) {
                if (root._isExpectedConsoleNoise(message))
                    return
                console.warn("js:", message)
            }
        }
    }
}
