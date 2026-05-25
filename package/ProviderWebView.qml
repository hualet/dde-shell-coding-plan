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

    signal closeRequested()
    signal extracted(real remainingRatio)
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
                    if (!root.isAllowedOrigin(webView.url)) {
                        root.extractionFailed(qsTr("This provider only allows quota reading on declared official domains."))
                        return
                    }

                    webView.runJavaScript(root.extractorScript, function(result) {
                        if (result && result.status === "ok" && result.remainingRatio >= 0) {
                            root.extracted(result.remainingRatio)
                        } else {
                            root.extractionFailed(result && result.message ? result.message : qsTr("Quota could not be read from this page."))
                        }
                    })
                }
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
        }
    }
}
