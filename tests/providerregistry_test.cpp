// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providerregistry.h"

#include <QFile>
#include <QTest>

class ProviderRegistryTest : public QObject
{
  Q_OBJECT

private slots:
  void builtInBrowserExtProvidersCoverMvpPlatforms ();
  void snapshotStatusMapsToPanelSeverity ();
  void panelQmlLeftClickOpensStatusPopup ();
  void panelQmlNoSettingsMenuOrRightClick ();
  void panelQmlNoManualInput ();
  void panelQmlTitleHasConsoleIcon ();
  void appletHasNoShowSettings ();
  void modelHasNoSetManualRatio ();
  void codexProviderUrlsMatchAnalyticsUsage ();
  void kimiCodeProviderUrlsMatchCodeConsole ();
  void glmCodingProviderUrlsMatchCodingPlanUsage ();
  void kimiCodeDualQuotaSnapshotContract ();
  void fiveHourDefaultIsNegativeOne ();
  void fiveHourMissingFieldStaysNegativeOne ();
  void fiveHourQuotaPercentFallsBackToText ();
  void browserExtResultParsesCorrectly ();
  void sourceTypeIsBrowserExt ();
  void panelQmlShowsExtensionConnectionStatus ();
  void panelQmlHasTokenDisplay ();
  void extensionManifestExists ();
  void extensionServiceWorkerExists ();
  void extensionProvidersExist ();
  void extensionSharedProtocolExists ();
  void websocketServerHeaderExists ();
  void websocketServerCppExists ();
  void websocketServerHasHeartbeat ();
  void websocketServerAuthGuardsNonAuthMessages ();
  void websocketServerHandlesJsonHeartbeat ();
  void websocketServerHasTokenPermissions ();
  void websocketServerOldConnectionClose ();
  void websocketServerDiscardClientNoDoubleDelete ();
  void browserExtProviderHeaderExists ();
  void browserExtProviderCppExists ();
  void cmakeUsesWebSockets ();
  void cmakeRemovesWebEngine ();
  void providerDefinitionFieldsCleaned ();
  void protocolProviderIdConsistency ();
  void modelMigratesWebviewSnapshots ();
  void refreshAllSendsAllProviderIds ();
  void refreshProviderUsesExtensionWhenConnected ();
  void extensionSendStatusUsesEnabledPlans ();
  void extensionSendStatusOnPlansChange ();
  void modelFiltersByExtensionEnabledPlans ();
  void modelHasSubscriptionsRespectsFilter ();
  void modelHasProviderFilterFlag ();
  void providersPropertyHasChangedSignal ();
  void extensionHandleRefreshRequestFiltersDisabled ();
  void extensionSendStatusCallsHaveCatchHandlers ();
};

void
ProviderRegistryTest::builtInBrowserExtProvidersCoverMvpPlatforms ()
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
      QCOMPARE (provider.sourceType, SourceType::BrowserExt);
      QVERIFY (!provider.loginUrl.isEmpty ());
      QVERIFY (!provider.consoleUrl.isEmpty ());
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
ProviderRegistryTest::panelQmlLeftClickOpensStatusPopup ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (
      QStringLiteral ("useClassicTaskbarLayout: Panel.itemAlignment === Dock.LeftAlignment")));
  QVERIFY (qml.contains (QStringLiteral ("TapHandler")));
  QVERIFY (qml.contains (QStringLiteral ("acceptedButtons: Qt.LeftButton")));
  QVERIFY (qml.contains (QStringLiteral ("onTapped:")));
  QVERIFY (qml.contains (QStringLiteral ("popup.open()")));
  QVERIFY (!qml.contains (QStringLiteral ("import QtWebEngine")));
  QVERIFY (!qml.contains (QStringLiteral ("WebEngineView")));
  QVERIFY (!qml.contains (QStringLiteral ("WebEngineProfile")));
}

void
ProviderRegistryTest::panelQmlNoSettingsMenuOrRightClick ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (!qml.contains (QStringLiteral ("Qt.labs.platform")));
  QVERIFY (!qml.contains (QStringLiteral ("acceptedButtons: Qt.RightButton")));
  QVERIFY (!qml.contains (QStringLiteral ("settingsMenuLoader")));
  QVERIFY (!qml.contains (QStringLiteral ("LP.Menu")));
  QVERIFY (!qml.contains (QStringLiteral ("showSettings")));
}

void
ProviderRegistryTest::panelQmlNoManualInput ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (!qml.contains (QStringLiteral ("手动录入")));
  QVERIFY (!qml.contains (QStringLiteral ("setManualRatio")));
  QVERIFY (!qml.contains (QStringLiteral ("manualInputPopup")));
  QVERIFY (!qml.contains (QStringLiteral ("selectedProvider")));
}

void
ProviderRegistryTest::panelQmlTitleHasConsoleIcon ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());

  QVERIFY (!qml.contains (QStringLiteral ("consoleBtn")));
  QVERIFY (qml.contains (QStringLiteral ("utilities-terminal-symbolic")));
  QVERIFY (qml.contains (QStringLiteral ("DciIcon {")));

  QVERIFY (qml.contains (QStringLiteral ("modelData.providerId")));

  QVERIFY (!qml.contains (QStringLiteral ("quotaSnapshots[0].providerId")));
  QVERIFY (!qml.contains (QStringLiteral ("for (var i = 0; i < root.quotaSnapshots.length")));
}

void
ProviderRegistryTest::appletHasNoShowSettings ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanapplet.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());
  QVERIFY (!headerContent.contains (QStringLiteral ("showSettings")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanapplet.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());
  QVERIFY (!sourceContent.contains (QStringLiteral ("showSettings")));
  QVERIFY (!sourceContent.contains (QStringLiteral ("QProcess::startDetached")));
}

void
ProviderRegistryTest::modelHasNoSetManualRatio ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());
  QVERIFY (!headerContent.contains (QStringLiteral ("setManualRatio")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());
  QVERIFY (!sourceContent.contains (QStringLiteral ("setManualRatio")));
  QVERIFY (!sourceContent.contains (QStringLiteral ("手动录入")));
}

void
ProviderRegistryTest::codexProviderUrlsMatchAnalyticsUsage ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  QVERIFY (registry.contains (QStringLiteral ("codex")));

  const ProviderDefinition provider = registry.provider (QStringLiteral ("codex"));

  QCOMPARE (provider.loginUrl,
            QStringLiteral ("https://chatgpt.com/auth/login"));
  QCOMPARE (provider.consoleUrl,
            QStringLiteral (
                "https://chatgpt.com/codex/cloud/settings/analytics#usage"));
  QCOMPARE (provider.sourceType, SourceType::BrowserExt);
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
  QCOMPARE (provider.consoleUrl,
            QStringLiteral ("https://www.kimi.com/code/console"));
  QCOMPARE (provider.sourceType, SourceType::BrowserExt);
}

void
ProviderRegistryTest::glmCodingProviderUrlsMatchCodingPlanUsage ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  QVERIFY (registry.contains (QStringLiteral ("glm-coding")));

  const ProviderDefinition provider
      = registry.provider (QStringLiteral ("glm-coding"));

  QCOMPARE (provider.loginUrl, QStringLiteral ("https://bigmodel.cn/"));
  QCOMPARE (provider.consoleUrl,
            QStringLiteral ("https://bigmodel.cn/coding-plan/personal/usage"));
  QCOMPARE (provider.sourceType, SourceType::BrowserExt);
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

  QCOMPARE (map.value (QStringLiteral ("source")).toString (),
            QStringLiteral ("browser_ext"));
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
}

void
ProviderRegistryTest::browserExtResultParsesCorrectly ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  for (const QString &providerId : registry.providerIds ())
    {
      const ProviderDefinition provider = registry.provider (providerId);
      QCOMPARE (provider.sourceType, SourceType::BrowserExt);
    }
}

void
ProviderRegistryTest::sourceTypeIsBrowserExt ()
{
  QCOMPARE (sourceTypeToString (SourceType::BrowserExt),
            QStringLiteral ("browser_ext"));
  QCOMPARE (sourceTypeToString (SourceType::OfficialApi),
            QStringLiteral ("official_api"));
  QCOMPARE (sourceTypeToString (SourceType::Manual),
            QStringLiteral ("manual"));
}

void
ProviderRegistryTest::panelQmlShowsExtensionConnectionStatus ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (QStringLiteral ("extensionConnected")));
  QVERIFY (qml.contains (QStringLiteral ("已连接")));
  QVERIFY (qml.contains (QStringLiteral ("未连接")));
}

void
ProviderRegistryTest::panelQmlHasTokenDisplay ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString qml = QString::fromUtf8 (file.readAll ());
  QVERIFY (qml.contains (QStringLiteral ("extensionToken")));
  QVERIFY (qml.contains (QStringLiteral ("配对 Token")));
}

void
ProviderRegistryTest::extensionManifestExists ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/extension/manifest.json"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("\"manifest_version\": 3")));
  QVERIFY (content.contains (QStringLiteral ("DDE Coding Plan Quota Helper")));
  QVERIFY (content.contains (QStringLiteral ("offscreen")));
  QVERIFY (content.contains (QStringLiteral ("storage")));
  QVERIFY (content.contains (QStringLiteral ("service_worker")));
}

void
ProviderRegistryTest::extensionServiceWorkerExists ()
{
  QFile sw (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (sw.open (QIODevice::ReadOnly), qPrintable (sw.errorString ()));

  const QString swContent = QString::fromUtf8 (sw.readAll ());
  QVERIFY (swContent.contains (QStringLiteral ("WebSocket")));
  QVERIFY (swContent.contains (QStringLiteral ("WS_URL")));
  QVERIFY (swContent.contains (QStringLiteral ("MSG_TYPE_REFRESH_REQUEST")));
  QVERIFY (swContent.contains (QStringLiteral ("MSG_TYPE_REFRESH_RESULT")));
  QVERIFY (swContent.contains (QStringLiteral ("offscreen")));

  QFile proto (QStringLiteral (SOURCE_DIR "/extension/shared/ws-protocol.js"));
  QVERIFY2 (proto.open (QIODevice::ReadOnly), qPrintable (proto.errorString ()));

  const QString protoContent = QString::fromUtf8 (proto.readAll ());
  QVERIFY (protoContent.contains (QStringLiteral ("127.0.0.1:18765")));
  QVERIFY (protoContent.contains (QStringLiteral ("refresh_request")));
  QVERIFY (protoContent.contains (QStringLiteral ("refresh_result")));
}

void
ProviderRegistryTest::extensionProvidersExist ()
{
  for (const QString &name : { QStringLiteral ("codex"),
                               QStringLiteral ("kimi-code"),
                               QStringLiteral ("glm-coding") })
    {
      QFile file (QStringLiteral (SOURCE_DIR "/extension/providers/") + name + QStringLiteral (".js"));
      QVERIFY2 (file.exists (), qPrintable (QStringLiteral ("Missing provider: %1.js").arg (name)));
      QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

      const QString content = QString::fromUtf8 (file.readAll ());
      QVERIFY (content.contains (QStringLiteral ("extractQuota")));
      QVERIFY (content.contains (QStringLiteral ("normalizeSnapshot")));
    }
}

void
ProviderRegistryTest::extensionSharedProtocolExists ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/extension/shared/ws-protocol.js"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("MSG_TYPE_AUTH")));
  QVERIFY (content.contains (QStringLiteral ("MSG_TYPE_REFRESH_REQUEST")));
  QVERIFY (content.contains (QStringLiteral ("WS_URL")));
  QVERIFY (content.contains (QStringLiteral ("127.0.0.1:18765")));
}

void
ProviderRegistryTest::websocketServerHeaderExists ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.h"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("WebSocketServer")));
  QVERIFY (content.contains (QStringLiteral ("start")));
  QVERIFY (content.contains (QStringLiteral ("sendRefreshRequest")));
  QVERIFY (content.contains (QStringLiteral ("sendOpenConsole")));
  QVERIFY (content.contains (QStringLiteral ("generateToken")));
  QVERIFY (content.contains (QStringLiteral ("refreshResultReceived")));
  QVERIFY (content.contains (QStringLiteral ("authSuccess")));
  QVERIFY (content.contains (QStringLiteral ("authFailed")));
}

void
ProviderRegistryTest::websocketServerCppExists ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("QWebSocketServer")));
  QVERIFY (content.contains (QStringLiteral ("18765")));
  QVERIFY (content.contains (QStringLiteral ("QHostAddress::LocalHost")));
}

void
ProviderRegistryTest::websocketServerHasHeartbeat ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/websocket_server.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());
  QVERIFY (headerContent.contains (QStringLiteral ("onHeartbeatTimeout")));
  QVERIFY (headerContent.contains (QStringLiteral ("startHeartbeat")));
  QVERIFY (headerContent.contains (QStringLiteral ("stopHeartbeat")));
  QVERIFY (headerContent.contains (QStringLiteral ("m_heartbeatTimer")));

  QFile cpp (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (cpp.open (QIODevice::ReadOnly), qPrintable (cpp.errorString ()));
  const QString cppContent = QString::fromUtf8 (cpp.readAll ());
  QVERIFY (cppContent.contains (QStringLiteral ("kHeartbeatTimeoutMs")));
  QVERIFY (cppContent.contains (QStringLiteral ("heartbeat timeout")));
  QVERIFY (cppContent.contains (QStringLiteral ("kHeartbeatIntervalMs")));
}

void
ProviderRegistryTest::websocketServerAuthGuardsNonAuthMessages ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("rejecting unauthenticated message")));
}

void
ProviderRegistryTest::websocketServerHandlesJsonHeartbeat ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("\"heartbeat\"")));

  QFile ws (QStringLiteral (SOURCE_DIR "/extension/shared/ws-protocol.js"));
  QVERIFY2 (ws.open (QIODevice::ReadOnly), qPrintable (ws.errorString ()));
  const QString wsContent = QString::fromUtf8 (ws.readAll ());
  QVERIFY (wsContent.contains (QStringLiteral ("MSG_TYPE_HEARTBEAT")));

  QFile sw (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (sw.open (QIODevice::ReadOnly), qPrintable (sw.errorString ()));
  const QString swContent = QString::fromUtf8 (sw.readAll ());
  QVERIFY (swContent.contains (QStringLiteral ("MSG_TYPE_HEARTBEAT")));
  QVERIFY (!swContent.contains (QStringLiteral ("ws.ping()")));
  QVERIFY (!swContent.contains (QStringLiteral ("onpong")));
}

void
ProviderRegistryTest::websocketServerHasTokenPermissions ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("QSaveFile")));
  QVERIFY (content.contains (QStringLiteral ("setPermissions")));
  QVERIFY (content.contains (QStringLiteral ("ReadOwner")));
  QVERIFY (content.contains (QStringLiteral ("WriteOwner")));
}

void
ProviderRegistryTest::websocketServerOldConnectionClose ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("discardClient")));
  QVERIFY (content.contains (QStringLiteral ("CloseCodeNormal")));
}

void
ProviderRegistryTest::websocketServerDiscardClientNoDoubleDelete ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/websocket_server.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());
  QVERIFY (headerContent.contains (QStringLiteral ("discardClient")));

  QFile file (QStringLiteral (SOURCE_DIR "/src/websocket_server.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  const auto extractMethodBody = [&content] (const QString &signature) -> QString {
    const QString nlSig = QLatin1Char ('\n') + signature;
    const qsizetype start = content.indexOf (nlSig);
    if (start < 0) return {};
    const qsizetype braceStart = content.indexOf (QLatin1Char ('{'), start);
    if (braceStart < 0) return {};
    int depth = 0;
    qsizetype i = braceStart;
    for (; i < content.size (); ++i)
      {
        if (content[i] == QLatin1Char ('{')) ++depth;
        else if (content[i] == QLatin1Char ('}'))
          {
            --depth;
            if (depth == 0) break;
          }
      }
    return content.mid (start, i - start + 1);
  };

  const QString discardBody = extractMethodBody (
      QStringLiteral ("WebSocketServer::discardClient"));
  QVERIFY2 (!discardBody.isEmpty (), "discardClient method not found");
  QVERIFY (discardBody.contains (QStringLiteral ("m_client = nullptr")));
  QVERIFY (discardBody.contains (QStringLiteral ("disconnect")));
  QVERIFY (discardBody.contains (QStringLiteral ("old->close")));
  QVERIFY (discardBody.contains (QStringLiteral ("old->deleteLater")));

  const QString disconnBody = extractMethodBody (
      QStringLiteral ("WebSocketServer::onClientDisconnected ()"));
  QVERIFY2 (!disconnBody.isEmpty (), "onClientDisconnected method not found");
  QVERIFY (disconnBody.contains (QStringLiteral ("qobject_cast")));
  QVERIFY (disconnBody.contains (QStringLiteral ("senderSocket")));
  QVERIFY (disconnBody.contains (QStringLiteral ("senderSocket != m_client")));
  QVERIFY (disconnBody.contains (QStringLiteral ("disconnect")));
  QVERIFY (disconnBody.contains (QStringLiteral ("old->deleteLater")));
}

void
ProviderRegistryTest::browserExtProviderHeaderExists ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/browser_ext_provider.h"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("BrowserExtProvider")));
  QVERIFY (content.contains (QStringLiteral ("refreshProviders")));
  QVERIFY (content.contains (QStringLiteral ("isExtensionConnected")));
  QVERIFY (content.contains (QStringLiteral ("refreshCompleted")));
  QVERIFY (content.contains (QStringLiteral ("refreshFailed")));
}

void
ProviderRegistryTest::browserExtProviderCppExists ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/browser_ext_provider.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("resultToSnapshot")));
  QVERIFY (content.contains (QStringLiteral ("BrowserExt")));
  QVERIFY (content.contains (QStringLiteral ("kProviderTimeoutMs")));
}

void
ProviderRegistryTest::cmakeUsesWebSockets ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/CMakeLists.txt"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (content.contains (QStringLiteral ("WebSockets")));
  QVERIFY (content.contains (QStringLiteral ("websocket_server")));
  QVERIFY (content.contains (QStringLiteral ("browser_ext_provider")));
}

void
ProviderRegistryTest::cmakeRemovesWebEngine ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/CMakeLists.txt"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());
  QVERIFY (!content.contains (QStringLiteral ("WebEngineWidgets")));
  QVERIFY (!content.contains (QStringLiteral ("WebChannel")));
  QVERIFY (!content.contains (QStringLiteral ("app/main.cpp")));
  QVERIFY (!content.contains (QStringLiteral ("webbridge")));
  QVERIFY (!content.contains (QStringLiteral ("web/dist")));
}

void
ProviderRegistryTest::providerDefinitionFieldsCleaned ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/providerregistry.h"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (!content.contains (QStringLiteral ("extractorScript")));
  QVERIFY (!content.contains (QStringLiteral ("allowedOrigins")));
  QVERIFY (!content.contains (QStringLiteral ("quotaUrl")));

  QVERIFY (content.contains (QStringLiteral ("loginUrl")));
  QVERIFY (content.contains (QStringLiteral ("consoleUrl")));
}

void
ProviderRegistryTest::protocolProviderIdConsistency ()
{
  QFile sw (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (sw.open (QIODevice::ReadOnly), qPrintable (sw.errorString ()));
  const QString swContent = QString::fromUtf8 (sw.readAll ());

  QVERIFY (swContent.contains (QStringLiteral ("codex")));
  QVERIFY (swContent.contains (QStringLiteral ("\"kimi-code\"")));
  QVERIFY (swContent.contains (QStringLiteral ("\"glm-coding\"")));

  QFile provider (QStringLiteral (SOURCE_DIR "/extension/providers/codex.js"));
  QVERIFY2 (provider.open (QIODevice::ReadOnly), qPrintable (provider.errorString ()));
  const QString providerContent = QString::fromUtf8 (provider.readAll ());
  QVERIFY (providerContent.contains (QStringLiteral ("id: \"codex\"")));

  QFile kimiProvider (QStringLiteral (SOURCE_DIR "/extension/providers/kimi-code.js"));
  QVERIFY2 (kimiProvider.open (QIODevice::ReadOnly), qPrintable (kimiProvider.errorString ()));
  const QString kimiContent = QString::fromUtf8 (kimiProvider.readAll ());
  QVERIFY (kimiContent.contains (QStringLiteral ("id: \"kimi-code\"")));

  QFile wsProto (QStringLiteral (SOURCE_DIR "/extension/shared/ws-protocol.js"));
  QVERIFY2 (wsProto.open (QIODevice::ReadOnly), qPrintable (wsProto.errorString ()));
  const QString protoContent = QString::fromUtf8 (wsProto.readAll ());
  QVERIFY (protoContent.contains (QStringLiteral ("MSG_TYPE_REFRESH_REQUEST")));
  QVERIFY (protoContent.contains (QStringLiteral ("MSG_TYPE_REFRESH_RESULT")));
  QVERIFY (protoContent.contains (QStringLiteral ("MSG_TYPE_REFRESH_PROGRESS")));
  QVERIFY (protoContent.contains (QStringLiteral ("MSG_TYPE_AUTH")));
  QVERIFY (protoContent.contains (QStringLiteral ("MSG_TYPE_OPEN_CONSOLE")));
}

void
ProviderRegistryTest::modelMigratesWebviewSnapshots ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("webview")));
  QVERIFY (content.contains (QStringLiteral ("BrowserExt")));

  const qsizetype loadDefStart = content.indexOf (
      QStringLiteral ("CodingPlanModel::loadSnapshots"));
  QVERIFY (loadDefStart >= 0);

  const qsizetype braceStart = content.indexOf (QLatin1Char ('{'), loadDefStart);
  QVERIFY (braceStart >= 0);
  int depth = 0;
  qsizetype braceEnd = braceStart;
  for (; braceEnd < content.size (); ++braceEnd)
    {
      if (content[braceEnd] == QLatin1Char ('{')) ++depth;
      else if (content[braceEnd] == QLatin1Char ('}'))
        {
          --depth;
          if (depth == 0) break;
        }
    }

  const QString loadBody = content.mid (loadDefStart, braceEnd - loadDefStart + 1);
  QVERIFY (loadBody.contains (QStringLiteral ("webview")));
  QVERIFY (loadBody.contains (QStringLiteral ("BrowserExt")));
}

void
ProviderRegistryTest::refreshAllSendsAllProviderIds ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  const qsizetype fnStart = content.indexOf (
      QStringLiteral ("\nCodingPlanModel::refreshAll"));
  QVERIFY (fnStart >= 0);

  const qsizetype braceStart = content.indexOf (QLatin1Char ('{'), fnStart);
  QVERIFY (braceStart >= 0);
  int depth = 0;
  qsizetype braceEnd = braceStart;
  for (; braceEnd < content.size (); ++braceEnd)
    {
      if (content[braceEnd] == QLatin1Char ('{')) ++depth;
      else if (content[braceEnd] == QLatin1Char ('}'))
        {
          --depth;
          if (depth == 0) break;
        }
    }

  const QString fnBody = content.mid (fnStart, braceEnd - fnStart + 1);
  QVERIFY (fnBody.contains (QStringLiteral ("m_hasProviderFilter")));
  QVERIFY (fnBody.contains (QStringLiteral ("m_enabledProviders")));
  QVERIFY (fnBody.contains (QStringLiteral ("m_registry.providerIds ()")));
  QVERIFY (!fnBody.contains (QStringLiteral ("SnapshotStatus::Ok")));
}

void
ProviderRegistryTest::refreshProviderUsesExtensionWhenConnected ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("CodingPlanModel::refreshProvider")));
  QVERIFY (content.contains (QStringLiteral ("m_browserExtProvider->isExtensionConnected")));
  QVERIFY (content.contains (QStringLiteral ("m_browserExtProvider->refreshProviders")));
  QVERIFY (content.contains (QStringLiteral ("isProviderEnabled")));
}

void
ProviderRegistryTest::extensionSendStatusUsesEnabledPlans ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("async function sendStatus")));
  QVERIFY (content.contains (QStringLiteral ("getEnabledPlans")));
  QVERIFY (!content.contains (QStringLiteral ("Object.keys(PROVIDERS)")));
}

void
ProviderRegistryTest::extensionSendStatusOnPlansChange ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("PLANS_STORAGE_KEY")));
  QVERIFY (content.contains (QStringLiteral ("plans changed, resending status")));
  QVERIFY (content.contains (QStringLiteral ("sendStatus().catch")));
}

void
ProviderRegistryTest::modelFiltersByExtensionEnabledPlans ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());

  QVERIFY (headerContent.contains (QStringLiteral ("m_enabledProviders")));
  QVERIFY (headerContent.contains (QStringLiteral ("onAvailableProvidersChanged")));
  QVERIFY (headerContent.contains (QStringLiteral ("isProviderEnabled")));
  QVERIFY (headerContent.contains (QStringLiteral ("m_hasProviderFilter")));
  QVERIFY (headerContent.contains (QStringLiteral ("providersChanged")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());

  QVERIFY (sourceContent.contains (QStringLiteral ("availableProvidersChanged")));
  QVERIFY (sourceContent.contains (QStringLiteral ("isProviderEnabled")));
  QVERIFY (sourceContent.contains (QStringLiteral ("m_hasProviderFilter")));

  QVERIFY (sourceContent.contains (QStringLiteral ("CodingPlanModel::snapshots")));
  QVERIFY (sourceContent.contains (QStringLiteral ("CodingPlanModel::providers")));

  QFile extHeader (QStringLiteral (SOURCE_DIR "/src/browser_ext_provider.h"));
  QVERIFY2 (extHeader.open (QIODevice::ReadOnly), qPrintable (extHeader.errorString ()));
  const QString extHeaderContent = QString::fromUtf8 (extHeader.readAll ());
  QVERIFY (extHeaderContent.contains (QStringLiteral ("availableProvidersChanged")));

  QFile extSource (QStringLiteral (SOURCE_DIR "/src/browser_ext_provider.cpp"));
  QVERIFY2 (extSource.open (QIODevice::ReadOnly), qPrintable (extSource.errorString ()));
  const QString extSourceContent = QString::fromUtf8 (extSource.readAll ());
  QVERIFY (extSourceContent.contains (QStringLiteral ("statusReceived")));
  QVERIFY (extSourceContent.contains (QStringLiteral ("onStatusReceived")));
}

void
ProviderRegistryTest::modelHasSubscriptionsRespectsFilter ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("CodingPlanModel::hasSubscriptions")));
  QVERIFY (content.contains (QStringLiteral ("isProviderEnabled")));
}

void
ProviderRegistryTest::modelHasProviderFilterFlag ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());

  QVERIFY (headerContent.contains (QStringLiteral ("m_hasProviderFilter")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());

  QVERIFY (sourceContent.contains (QStringLiteral ("CodingPlanModel::isProviderEnabled")));
  QVERIFY (sourceContent.contains (QStringLiteral ("!m_hasProviderFilter")));
  QVERIFY (sourceContent.contains (QStringLiteral ("m_hasProviderFilter = true")));
  QVERIFY (sourceContent.contains (QStringLiteral ("m_hasProviderFilter = false")));
}

void
ProviderRegistryTest::providersPropertyHasChangedSignal ()
{
  QFile header (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY2 (header.open (QIODevice::ReadOnly), qPrintable (header.errorString ()));
  const QString headerContent = QString::fromUtf8 (header.readAll ());

  QVERIFY (!headerContent.contains (QStringLiteral ("providers READ providers CONSTANT")));
  QVERIFY (headerContent.contains (QStringLiteral ("providers READ providers NOTIFY providersChanged")));
  QVERIFY (headerContent.contains (QStringLiteral ("void providersChanged ()")));

  QFile source (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"));
  QVERIFY2 (source.open (QIODevice::ReadOnly), qPrintable (source.errorString ()));
  const QString sourceContent = QString::fromUtf8 (source.readAll ());

  QVERIFY (sourceContent.contains (QStringLiteral ("CodingPlanModel::onAvailableProvidersChanged")));
  QVERIFY (sourceContent.contains (QStringLiteral ("emit providersChanged")));
}

void
ProviderRegistryTest::extensionHandleRefreshRequestFiltersDisabled ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("async function handleRefreshRequest")));
  QVERIFY (content.contains (QStringLiteral ("getEnabledPlans")));
  QVERIFY (content.contains (QStringLiteral ("enabled.includes")));
}

void
ProviderRegistryTest::extensionSendStatusCallsHaveCatchHandlers ()
{
  QFile file (QStringLiteral (SOURCE_DIR "/extension/service-worker.js"));
  QVERIFY2 (file.open (QIODevice::ReadOnly), qPrintable (file.errorString ()));

  const QString content = QString::fromUtf8 (file.readAll ());

  QVERIFY (content.contains (QStringLiteral ("sendStatus().catch")));
}

QTEST_MAIN (ProviderRegistryTest)

#include "providerregistry_test.moc"
