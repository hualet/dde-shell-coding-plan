// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providerregistry.h"

#include <QFile>
#include <QTest>

class ProviderRegistryTest : public QObject
{
  Q_OBJECT

private slots:
  void builtInWebViewProvidersCoverMvpPlatforms ();
  void snapshotStatusMapsToPanelSeverity ();
  void panelQmlLeftClickOpensStatusPopupWithoutWebEngine ();
  void panelQmlRightClickOpensSettingsMenu ();
  void appletLaunchesStandaloneSettingsWindow ();
  void standaloneAppSourceFilesExist ();
  void reactFrontendEntryExists ();
  void kimiCodeProviderUrlsMatchCodeConsole ();
  void kimiCodeExtractorReadsBillingUsageApi ();
  void kimiCodeDualQuotaSnapshotContract ();
  void fiveHourDefaultIsNegativeOne ();
  void kimiCodeExtractorOmitsFieldsOnNull ();
  void fiveHourMissingFieldStaysNegativeOne ();
  void fiveHourQuotaPercentFallsBackToText ();
  void webViewAcceptsTextOnlyResult ();
  void webPopupReturnsToStatusAfterAutoExtractionFailure ();
  void loginPageDetectionBlocksPrematureExtraction ();
  void mainCppLoginGuardBlocksReload ();
  void standaloneLoginPageProbesQuotaUrlBeforeWaiting ();
  void nullResultDoesNotCrashAndEntersFailureCallback ();
};

void
ProviderRegistryTest::builtInWebViewProvidersCoverMvpPlatforms ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();

  const QStringList providerIds = registry.providerIds ();
  QCOMPARE (providerIds.size (), 3);
  QVERIFY (providerIds.contains (QStringLiteral ("codex")));
  QVERIFY (providerIds.contains (QStringLiteral ("kimi-code")));
  QVERIFY (providerIds.contains (QStringLiteral ("glm-coding")));

  for (const QString &providerId : providerIds)
    {
      const ProviderDefinition provider = registry.provider (providerId);
      QCOMPARE (provider.sourceType, SourceType::WebView);
      QVERIFY (!provider.loginUrl.isEmpty ());
      QVERIFY (!provider.quotaUrl.isEmpty ());
      QVERIFY (!provider.consoleUrl.isEmpty ());
      QVERIFY (!provider.allowedOrigins.isEmpty ());
    }
}

void
ProviderRegistryTest::snapshotStatusMapsToPanelSeverity ()
{
  QuotaSnapshot snapshot;
  snapshot.status = SnapshotStatus::Ok;
  snapshot.remainingRatio = 0.31;
  QCOMPARE (snapshot.severity (), PanelSeverity::Normal);

  snapshot.remainingRatio = 0.3;
  QCOMPARE (snapshot.severity (), PanelSeverity::Warning);

  snapshot.remainingRatio = 0.09;
  QCOMPARE (snapshot.severity (), PanelSeverity::Critical);

  snapshot.status = SnapshotStatus::ParseError;
  snapshot.remainingRatio = 0.9;
  QCOMPARE (snapshot.severity (), PanelSeverity::Error);
}

void
ProviderRegistryTest::panelQmlLeftClickOpensStatusPopupWithoutWebEngine ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (
      QStringLiteral ("useClassicTaskbarLayout: Panel.itemAlignment === Dock.LeftAlignment")));
  QVERIFY (qml.contains (
      QStringLiteral ("dockOrder: useClassicTaskbarLayout ? 21 : 10")));
  QVERIFY (qml.contains (QStringLiteral ("function openLoginCenter")));
  QVERIFY (qml.contains (QStringLiteral ("TapHandler")));
  QVERIFY (qml.contains (QStringLiteral ("acceptedButtons: Qt.LeftButton")));
  QVERIFY (qml.contains (QStringLiteral ("onTapped:")));
  QVERIFY (qml.contains (QStringLiteral ("popup.open()")));
  QVERIFY (qml.contains (QStringLiteral ("webPopup.open()")));
  QVERIFY (!qml.contains (QStringLiteral (
      "Panel.requestClosePopup()\n                root.openLoginCenter()")));
  QVERIFY (!qml.contains (QStringLiteral ("webPopup.popupVisible = true")));
  QVERIFY (!qml.contains (QStringLiteral (
      "MouseArea {\n        anchors.fill: parent\n        onClicked: root.openLoginCenter()")));

  const qsizetype popupStart = qml.indexOf (QStringLiteral ("id: popup"));
  const qsizetype webPopupStart = qml.indexOf (QStringLiteral ("id: webPopup"));
  QVERIFY (popupStart >= 0);
  QVERIFY (webPopupStart > popupStart);

  const QString statusPopup = qml.mid (popupStart, webPopupStart - popupStart);
  QVERIFY (statusPopup.contains (QStringLiteral ("5小时额度")));
  QVERIFY (statusPopup.contains (QStringLiteral ("周额度")));
  QVERIFY (statusPopup.contains (QStringLiteral ("anchors.centerIn: parent")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Refresh\")")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Login\")")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Console\")")));
  QVERIFY (!statusPopup.contains (QStringLiteral ("text: qsTr(\"Manual 50%\")")));
}

void
ProviderRegistryTest::panelQmlRightClickOpensSettingsMenu ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (QStringLiteral ("import Qt.labs.platform 1.1 as LP")));
  QVERIFY (qml.contains (QStringLiteral ("acceptedButtons: Qt.RightButton")));
  QVERIFY (qml.contains (QStringLiteral ("settingsMenuLoader.item.open()")));
  QVERIFY (qml.contains (QStringLiteral ("text: qsTr(\"设置\")")));
  QVERIFY (qml.contains (QStringLiteral ("Applet.showSettings()")));
}

void
ProviderRegistryTest::appletLaunchesStandaloneSettingsWindow ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanapplet.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());
  QVERIFY (headerContent.contains (QStringLiteral ("Q_INVOKABLE")));
  QVERIFY (headerContent.contains (QStringLiteral ("showSettings")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanapplet.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());
  QVERIFY (sourceContent.contains (QStringLiteral ("QProcess::startDetached")));
  QVERIFY (sourceContent.contains (QStringLiteral ("/usr/bin/dde-coding-plan")));
  QVERIFY (sourceContent.contains (QStringLiteral ("dde-coding-plan")));
}

void
ProviderRegistryTest::standaloneAppSourceFilesExist ()
{
  QFile mainCpp (QStringLiteral (SOURCE_DIR "/app/main.cpp"));
  QVERIFY2 (mainCpp.exists (), "app/main.cpp should exist");
  QVERIFY2 (mainCpp.open (QIODevice::ReadOnly), "app/main.cpp should be readable");

  QFile webbridge (QStringLiteral (SOURCE_DIR "/app/webbridge.h"));
  QVERIFY2 (webbridge.exists (), "app/webbridge.h should exist");

  const QString content = QString::fromUtf8 (mainCpp.readAll ());
  QVERIFY (content.contains (QStringLiteral ("QWebEngineView")));
  QVERIFY (content.contains (QStringLiteral ("QWebChannel")));
  QVERIFY (content.contains (QStringLiteral ("WebBridge")));
  QVERIFY (content.contains (QStringLiteral ("DMainWindow")));
  QVERIFY (content.contains (QStringLiteral ("runJavaScript")));
  QVERIFY (content.contains (QStringLiteral ("allowedOrigins")));
  QVERIFY (content.contains (QStringLiteral ("setWebViewResult")));
  QVERIFY (content.contains (QStringLiteral ("CODING_PLAN_WEB_DIR")));
  QVERIFY (content.contains (QStringLiteral ("/usr/share/dde-coding-plan/web")));
  QVERIFY (!content.contains (QStringLiteral ("qrc:/web/index.html")));
  QVERIFY (!content.contains (QStringLiteral ("loadTranslator()")));
}

void
ProviderRegistryTest::reactFrontendEntryExists ()
{
  QFile indexHtml (QStringLiteral (SOURCE_DIR "/web/index.html"));
  QVERIFY2 (indexHtml.open (QIODevice::ReadOnly), "web/index.html should be readable");
  const QString indexContent = QString::fromUtf8 (indexHtml.readAll ());
  QVERIFY (indexContent.contains (
      QStringLiteral ("./qwebchannel.js")));

  QFile packageJson (QStringLiteral (SOURCE_DIR "/web/package.json"));
  QVERIFY2 (packageJson.open (QIODevice::ReadOnly), "web/package.json should be readable");

  const QString content = QString::fromUtf8 (packageJson.readAll ());
  QVERIFY (content.contains (QStringLiteral ("@mui/material")));
  QVERIFY (content.contains (QStringLiteral ("react")));
  QVERIFY (content.contains (QStringLiteral ("react-router-dom")));

  QFile cmakeLists (QStringLiteral (SOURCE_DIR "/CMakeLists.txt"));
  QVERIFY2 (cmakeLists.open (QIODevice::ReadOnly), "CMakeLists.txt should be readable");

  const QString cmake = QString::fromUtf8 (cmakeLists.readAll ());
  QVERIFY (cmake.contains (QStringLiteral ("CODING_PLAN_WEB_DIR")));
  QVERIFY (cmake.contains (QStringLiteral ("install(")));
  QVERIFY (cmake.contains (QStringLiteral ("web/dist/")));
}

void
ProviderRegistryTest::kimiCodeProviderUrlsMatchCodeConsole ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  QVERIFY (registry.contains (QStringLiteral ("kimi-code")));

  const ProviderDefinition provider
      = registry.provider (QStringLiteral ("kimi-code"));

  QCOMPARE (provider.loginUrl,
            QStringLiteral ("https://www.kimi.com/code/"));
  QCOMPARE (provider.quotaUrl,
            QStringLiteral ("https://www.kimi.com/code/console"));
  QCOMPARE (provider.consoleUrl,
            QStringLiteral ("https://www.kimi.com/code/console"));

  QVERIFY (provider.allowedOrigins.contains (
      QStringLiteral ("https://www.kimi.com")));
  QCOMPARE (provider.sourceType, SourceType::WebView);
  QVERIFY (!provider.extractorScript.isEmpty ());
}

void
ProviderRegistryTest::kimiCodeExtractorReadsBillingUsageApi ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  const ProviderDefinition provider
      = registry.provider (QStringLiteral ("kimi-code"));

  const QString script = provider.extractorScript;

  QVERIFY (script.contains (QStringLiteral (
      "kimi.gateway.billing.v1.BillingService/GetUsages")));
  QVERIFY (script.contains (QStringLiteral ("FEATURE_CODING")));
  QVERIFY (script.contains (QStringLiteral ("localStorage.getItem('access_token')")));
  QVERIFY (script.contains (QStringLiteral ("usage.detail")));
  QVERIFY (script.contains (QStringLiteral ("usage.limits")));
  QVERIFY (script.contains (QStringLiteral ("detail.remaining")));
  QVERIFY (script.contains (QStringLiteral ("limit - used")));
  QVERIFY (script.contains (QStringLiteral ("usedRatio")));
  QVERIFY (script.contains (QStringLiteral ("remainingRatio")));
  QVERIFY (script.contains (QStringLiteral ("fiveHour")));
  QVERIFY (!script.contains (QStringLiteral ("stats-section")));
  QVERIFY (!script.contains (QStringLiteral ("nth-child")));
  QVERIFY (script.contains (QStringLiteral ("kimi-code")));
  QVERIFY (script.contains (QStringLiteral ("fiveHourBalanceText")));
  QVERIFY (script.contains (QStringLiteral ("fiveHourRemainingRatio")));
  QVERIFY (script.contains (QStringLiteral ("parse_error")));
}

void
ProviderRegistryTest::kimiCodeDualQuotaSnapshotContract ()
{
  QuotaSnapshot snapshot;
  snapshot.providerId = QStringLiteral ("kimi-code");
  snapshot.providerName = QStringLiteral ("Kimi Code");
  snapshot.status = SnapshotStatus::Ok;
  snapshot.remainingRatio = 0.75;
  snapshot.balanceText = QStringLiteral ("75%");
  snapshot.fiveHourRemainingRatio = 0.40;
  snapshot.fiveHourBalanceText = QStringLiteral ("40%");

  const QVariantMap map = snapshot.toVariantMap ();

  QCOMPARE (map.value (QStringLiteral ("providerId")).toString (),
            QStringLiteral ("kimi-code"));
  QCOMPARE (map.value (QStringLiteral ("remainingRatio")).toDouble (), 0.75);
  QCOMPARE (map.value (QStringLiteral ("balanceText")).toString (),
            QStringLiteral ("75%"));
  QCOMPARE (map.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (), 0.40);
  QCOMPARE (map.value (QStringLiteral ("fiveHourBalanceText")).toString (),
            QStringLiteral ("40%"));

  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  const ProviderDefinition provider
      = registry.provider (QStringLiteral ("kimi-code"));
  const QString script = provider.extractorScript;

  QVERIFY (script.contains (QStringLiteral ("fiveHourRemainingRatio")));
  QVERIFY (script.contains (QStringLiteral ("remainingRatio")));
  QVERIFY (script.contains (QStringLiteral ("weekly")));
  QVERIFY (script.contains (QStringLiteral ("fiveHour")));
}

void
ProviderRegistryTest::fiveHourDefaultIsNegativeOne ()
{
  QuotaSnapshot snapshot;
  QCOMPARE (snapshot.fiveHourRemainingRatio, -1.0);

  const QVariantMap map = snapshot.toVariantMap ();
  QVERIFY (map.contains (QStringLiteral ("fiveHourRemainingRatio")));
  QCOMPARE (map.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (), -1.0);
  QCOMPARE (map.value (QStringLiteral ("fiveHourBalanceText")).toString (),
            QString ());
}

void
ProviderRegistryTest::kimiCodeExtractorOmitsFieldsOnNull ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  const ProviderDefinition provider
      = registry.provider (QStringLiteral ("kimi-code"));
  const QString script = provider.extractorScript;

  QVERIFY (script.contains (QStringLiteral ("if (parsed.weekly)")));
  QVERIFY (script.contains (QStringLiteral ("if (parsed.fiveHour)")));

  QVERIFY (!script.contains (QStringLiteral ("fiveHourRemainingRatio: 0")));
  QVERIFY (!script.contains (QStringLiteral ("remainingRatio: 0")));
}

void
ProviderRegistryTest::fiveHourMissingFieldStaysNegativeOne ()
{
  QuotaSnapshot snapshot;
  snapshot.providerId = QStringLiteral ("codex");
  snapshot.providerName = QStringLiteral ("Codex / ChatGPT");
  snapshot.status = SnapshotStatus::Ok;
  snapshot.remainingRatio = 0.80;
  snapshot.balanceText = QStringLiteral ("80%");
  snapshot.fiveHourRemainingRatio = -1.0;
  snapshot.fiveHourBalanceText = QString ();

  const QVariantMap map = snapshot.toVariantMap ();

  QCOMPARE (map.value (QStringLiteral ("remainingRatio")).toDouble (), 0.80);
  QCOMPARE (map.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (), -1.0);
  QVERIFY (map.value (QStringLiteral ("fiveHourBalanceText")).toString ().isEmpty ());

  QuotaSnapshot roundTripped;
  roundTripped.status = SnapshotStatus::Ok;
  roundTripped.remainingRatio = map.value (QStringLiteral ("remainingRatio")).toDouble ();
  roundTripped.fiveHourRemainingRatio = map.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble ();
  roundTripped.fiveHourBalanceText = map.value (QStringLiteral ("fiveHourBalanceText")).toString ();

  QCOMPARE (roundTripped.remainingRatio, 0.80);
  QCOMPARE (roundTripped.fiveHourRemainingRatio, -1.0);
  QVERIFY (roundTripped.fiveHourBalanceText.isEmpty ());
}

void
ProviderRegistryTest::fiveHourQuotaPercentFallsBackToText ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());

  QVERIFY (qml.contains (QStringLiteral ("function fiveHourQuotaPercent(snapshot)")));
  QVERIFY (qml.contains (QStringLiteral ("raw < 0")));

  const qsizetype fnStart = qml.indexOf (QStringLiteral ("function fiveHourQuotaPercent"));
  QVERIFY (fnStart >= 0);
  const qsizetype fnEnd = qml.indexOf (QLatin1Char ('}'), qml.indexOf (QLatin1Char ('}'), fnStart + 1) + 1);
  const QString fnBody = qml.mid (fnStart, fnEnd - fnStart + 1);

  QVERIFY (fnBody.contains (QStringLiteral ("N/A")));
  QVERIFY (fnBody.contains (QStringLiteral ("fiveHourBalanceText")));

  QVERIFY (!fnBody.contains (QStringLiteral ("Math.max(0, Math.min(1, snapshot.fiveHourRemainingRatio")));
}

void
ProviderRegistryTest::webViewAcceptsTextOnlyResult ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/ProviderWebView.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());

  QVERIFY (!qml.contains (QStringLiteral (
      "result.remainingRatio >= 0 || result.fiveHourRemainingRatio >= 0")));

  QVERIFY (qml.contains (QStringLiteral ("var hasRatio")));
  QVERIFY (qml.contains (QStringLiteral ("var hasText")));
  QVERIFY (qml.contains (QStringLiteral ("balanceText")));
  QVERIFY (qml.contains (QStringLiteral ("fiveHourBalanceText")));
  QVERIFY (qml.contains (QStringLiteral ("hasRatio || hasText")));

  QVERIFY (qml.contains (QStringLiteral ("typeof result.remainingRatio")));
  QVERIFY (qml.contains (QStringLiteral ("typeof result.balanceText")));
}

void
ProviderRegistryTest::webPopupReturnsToStatusAfterAutoExtractionFailure ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());

  const qsizetype failedStart = qml.indexOf (
      QStringLiteral ("item.extractionFailed.connect"));
  QVERIFY (failedStart >= 0);

  const qsizetype failedEnd = qml.indexOf (
      QStringLiteral ("item.loginSucceeded.connect"), failedStart);
  QVERIFY (failedEnd > failedStart);

  const QString failedHandler = qml.mid (failedStart, failedEnd - failedStart);
  QVERIFY (failedHandler.contains (QStringLiteral ("setProviderError")));
  QVERIFY (failedHandler.contains (QStringLiteral ("item._wasAutoMode")));
  QVERIFY (failedHandler.contains (QStringLiteral ("autoCloseTimer.start()")));
}

void
ProviderRegistryTest::loginPageDetectionBlocksPrematureExtraction ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/ProviderWebView.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());

  QVERIFY (qml.contains (QStringLiteral ("_isOnLoginPage")));
  QVERIFY (qml.contains (QStringLiteral ("_urlBasePath")));

  QVERIFY (!qml.contains (QStringLiteral (
      "indexOf(\"/login\") === -1")));

  QVERIFY (qml.contains (QStringLiteral (
      "!root._isOnLoginPage(webView.url)")));

  const qsizetype phaseOneStart = qml.indexOf (
      QStringLiteral ("_autoPhase === 1"));
  QVERIFY (phaseOneStart >= 0);
  const qsizetype phaseOneEnd = qml.indexOf (
      QStringLiteral ("} else if"), phaseOneStart);
  QVERIFY (phaseOneEnd > phaseOneStart);

  const QString phaseOne = qml.mid (phaseOneStart, phaseOneEnd - phaseOneStart);
  QVERIFY (phaseOne.contains (QStringLiteral ("_isOnLoginPage")));
}

void
ProviderRegistryTest::mainCppLoginGuardBlocksReload ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/app/main.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString cpp = QString::fromUtf8 (file.readAll ());

  QVERIFY (cpp.contains (QStringLiteral ("onLoginPage && m_loginPhase == 0")));
  QVERIFY (cpp.contains (QStringLiteral ("onLoginPage)")));
  QVERIFY (cpp.contains (QStringLiteral ("QUrl(loginPath).path()")));
}

void
ProviderRegistryTest::standaloneLoginPageProbesQuotaUrlBeforeWaiting ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/app/main.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString cpp = QString::fromUtf8 (file.readAll ());

  const qsizetype guardStart = cpp.indexOf (
      QStringLiteral ("onLoginPage && m_loginPhase == 0"));
  QVERIFY (guardStart >= 0);

  const qsizetype guardEnd = cpp.indexOf (
      QStringLiteral ("if (onLoginPage)"), guardStart);
  QVERIFY (guardEnd > guardStart);

  const QString guardBlock = cpp.mid (guardStart, guardEnd - guardStart);
  QVERIFY (guardBlock.contains (QStringLiteral ("QTimer::singleShot")));
  QVERIFY (guardBlock.contains (QStringLiteral ("quotaUrl")));
  QVERIFY (guardBlock.contains (QStringLiteral ("setUrl(QUrl(quotaUrl))")));
}

QTEST_MAIN (ProviderRegistryTest)

#include "providerregistry_test.moc"

void
ProviderRegistryTest::nullResultDoesNotCrashAndEntersFailureCallback ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/ProviderWebView.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());

  const qsizetype runJsStart = qml.indexOf (QStringLiteral ("runJavaScript"));
  QVERIFY (runJsStart >= 0);

  const qsizetype callbackStart = qml.indexOf (QStringLiteral ("function(result)"), runJsStart);
  QVERIFY (callbackStart >= 0);

  const qsizetype callbackEnd = qml.indexOf (QStringLiteral ("})"), callbackStart);
  QVERIFY (callbackEnd > callbackStart);

  const QString callback = qml.mid (callbackStart, callbackEnd - callbackStart + 2);

  QVERIFY (callback.contains (QStringLiteral (
      "if (!result || typeof result !== \"object\")")));

  const qsizetype guardPos = callback.indexOf (
      QStringLiteral ("if (!result || typeof result !== \"object\")"));
  QVERIFY (guardPos >= 0);

  const qsizetype hasRatioPos = callback.indexOf (
      QStringLiteral ("var hasRatio"));
  QVERIFY (hasRatioPos >= 0);
  QVERIFY (hasRatioPos > guardPos);

  const qsizetype extractionFailedPos = callback.indexOf (
      QStringLiteral ("extractionFailed"), guardPos);
  QVERIFY (extractionFailedPos >= 0);
  QVERIFY (extractionFailedPos < hasRatioPos);

  QVERIFY (callback.contains (QStringLiteral ("_autoMode = false")));
}
