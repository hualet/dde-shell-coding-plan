// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "credential_store.h"
#include "codingplanmodel.h"
#include "direct_quota_provider.h"
#include "providerregistry.h"
#include "quota_parsers.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <functional>

class ProviderRegistryTest : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase ();
  void cleanupTestCase ();

  // Registry / data model contracts
  void builtInProvidersCoverMvpPlatforms ();
  void snapshotStatusMapsToPanelSeverity ();
  void snapshotVariantMapCarriesEntryIdentity ();
  void dualQuotaDefaultsAreUnknown ();
  void providerUrlsMatchOfficialConsoles ();

  // Credential readers
  void codexCredentialsReaderHandlesModes ();
  void kimiCredentialsReaderCoercesSeconds ();
  void zcodeReaderPicksActiveCodingPlanKey ();
  void zcodeReaderFallsBackToAnyCodingPlanKey ();

  // Parsers (fixtures shaped after the vendors' real replies)
  void codexParserMapsWindowsAndPlan ();
  void codexParserSwapsWindowRolesByDuration ();
  void codexParserRejectsEmptyRateLimit ();
  void codexParserClassifiesLoneWindowByLength ();
  void kimiParserMapsWeeklyAndSession ();
  void kimiParserHandlesStringNumbers ();
  void glmParserMapsUnitWindows ();
  void glmParserRejectsFailedReply ();
  void glmParserClassifiesLoneWindowByLength ();
  void minimaxParserMapsGeneralBucket ();
  void minimaxParserTreats1004AsAuthError ();
  void minimaxParserRejectsMissingPercentages ();
  void epochHelperAdaptsSecondsAndMilliseconds ();
  void usedPercentHelperClamps ();
  void jwtExpiryDecodesPayload ();

  // Codex token write-back
  void codexTokenWriteBackPreservesFieldsAndPermissions ();

  // Model: entries, manual accounts, persistence
  void autoEntriesFollowCredentialProbe ();
  void manualAccountRoundTrip ();
  void manualAccountsSupportDuplicatesPerVendor ();
  void providerKeyStoreIsOwnerOnly ();
  void legacySnapshotsMigrateToAutoEntries ();
  void modelTooltipUsesFiveHourQuota ();
  void modelTooltipFallsBackToWeeklyQuota ();
  void refreshFailureKeepsRecentGoodSnapshot ();
  void refreshFailureReplacesStaleSnapshot ();
  void refreshFailureKeepWindowDoesNotSlide ();
  void authFailureIsNeverMaskedByStaleSnapshot ();
  void snapshotsPersistResetAndCounts ();
  void modelAutoRefreshIsCppSide ();

  // Panel / build structure contracts
  void panelQmlLeftClickOpensStatusPopup ();
  void panelQmlHasAccountManagement ();
  void panelQmlHasNoExtensionPairingUi ();
  void panelQmlFiveHourFallsBackToText ();
  void panelQmlDockWidgetUsesOneFiveHourRingPerProvider ();
  void cmakeUsesQtNetworkWithoutWebSockets ();
  void directProviderUsesOfficialEndpoints ();
  void directProviderSendsCodexCliIdentity ();
  void kimiPolicyIsReadOnly ();
  void directProviderFailsOverToFullGlmUrl ();
  void directProviderGuardsCodexTokenRotation ();
  void directProviderClearsInFlightOnSyncFailure ();
  void modelHasNoSelfTriggeredSettingsWatcher ();
  void appletStartsAutoRefresh ();

private:
  static QString readFile (const QString &path);
  static void writeFile (const QString &path, const QString &content);
  static QJsonObject parseJson (const QString &json);
  void resetPersistence ();

  std::unique_ptr<QTemporaryDir> m_sandbox;
};

void
ProviderRegistryTest::initTestCase ()
{
  m_sandbox.reset (new QTemporaryDir);
  QVERIFY (m_sandbox->isValid ());
  // Redirect everything the model persists (QSettings file, provider key
  // store) into a sandbox before the first QSettings/QStandardPaths use.
  qputenv ("XDG_CONFIG_HOME", m_sandbox->path ().toUtf8 ());
  QSettings::setPath (QSettings::NativeFormat, QSettings::UserScope,
                      m_sandbox->path ());
  qRegisterMetaType<SnapshotStatus> ();
}

void
ProviderRegistryTest::cleanupTestCase ()
{
  m_sandbox.reset ();
}

QString
ProviderRegistryTest::readFile (const QString &path)
{
  QFile file (path);
  if (!file.open (QIODevice::ReadOnly))
    {
      QTest::qFail (qPrintable (QStringLiteral ("cannot open %1: %2")
                                    .arg (path, file.errorString ())),
                    __FILE__, __LINE__);
      return QString ();
    }
  return QString::fromUtf8 (file.readAll ());
}

void
ProviderRegistryTest::writeFile (const QString &path, const QString &content)
{
  QFile file (path);
  QVERIFY2 (file.open (QIODevice::WriteOnly), qPrintable (file.errorString ()));
  file.write (content.toUtf8 ());
  file.close ();
}

// Tests share one sandboxed QSettings; wipe the persisted model state so a
// test starts from a clean slate.
void
ProviderRegistryTest::resetPersistence ()
{
  QSettings settings (QStringLiteral ("deepin"),
                      QStringLiteral ("dde-shell-coding-plan"));
  settings.remove (QStringLiteral ("snapshots"));
  settings.remove (QStringLiteral ("manualAccounts"));
  settings.sync ();
  QFile::remove (QStandardPaths::writableLocation (
                     QStandardPaths::GenericConfigLocation)
                 + QStringLiteral ("/dde-shell-coding-plan/provider-keys.json"));
}

QJsonObject
ProviderRegistryTest::parseJson (const QString &json)
{
  const QJsonDocument document = QJsonDocument::fromJson (json.toUtf8 ());
  Q_ASSERT (document.isObject ());
  return document.object ();
}

// ---------------------------------------------------------------------------
// Registry / data model contracts
// ---------------------------------------------------------------------------

void
ProviderRegistryTest::builtInProvidersCoverMvpPlatforms ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();

  const QStringList providerIds = registry.providerIds ();
  QCOMPARE (providerIds.size (), 4);
  QVERIFY (providerIds.contains (QStringLiteral ("codex")));
  QVERIFY (providerIds.contains (QStringLiteral ("kimi-code")));
  QVERIFY (providerIds.contains (QStringLiteral ("glm-coding")));
  QVERIFY (providerIds.contains (QStringLiteral ("minimax")));

  for (const QString &providerId : providerIds)
    {
      const ProviderDefinition provider = registry.provider (providerId);
      QCOMPARE (provider.sourceType, SourceType::OfficialApi);
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
ProviderRegistryTest::snapshotVariantMapCarriesEntryIdentity ()
{
  QuotaSnapshot snapshot;
  snapshot.entryId = QStringLiteral ("manual:abc");
  snapshot.label = QStringLiteral ("工作号");
  snapshot.providerId = QStringLiteral ("minimax");
  snapshot.providerName = QStringLiteral ("MiniMax Coding");
  snapshot.source = SourceType::Manual;
  snapshot.status = SnapshotStatus::Ok;
  snapshot.remainingRatio = 0.5;

  const QVariantMap map = snapshot.toVariantMap ();
  QCOMPARE (map.value (QStringLiteral ("entryId")).toString (),
            QStringLiteral ("manual:abc"));
  QCOMPARE (map.value (QStringLiteral ("label")).toString (),
            QStringLiteral ("工作号"));
  QCOMPARE (map.value (QStringLiteral ("providerId")).toString (),
            QStringLiteral ("minimax"));
  QCOMPARE (map.value (QStringLiteral ("source")).toString (),
            QStringLiteral ("manual"));
  QCOMPARE (map.value (QStringLiteral ("severity")).toString (),
            QStringLiteral ("normal"));
}

void
ProviderRegistryTest::dualQuotaDefaultsAreUnknown ()
{
  QuotaSnapshot snapshot;
  QCOMPARE (snapshot.remainingRatio, -1.0);
  QCOMPARE (snapshot.fiveHourRemainingRatio, -1.0);
  QCOMPARE (snapshot.status, SnapshotStatus::Unsupported);
}

void
ProviderRegistryTest::providerUrlsMatchOfficialConsoles ()
{
  const ProviderRegistry registry = ProviderRegistry::createDefault ();
  QCOMPARE (registry.provider (QStringLiteral ("codex")).consoleUrl,
            QStringLiteral ("https://chatgpt.com/settings/usage?tab=overview"));
  QCOMPARE (registry.provider (QStringLiteral ("kimi-code")).consoleUrl,
            QStringLiteral ("https://www.kimi.com/code/console"));
  QCOMPARE (registry.provider (QStringLiteral ("glm-coding")).consoleUrl,
            QStringLiteral ("https://bigmodel.cn/coding-plan/personal/usage"));
  QCOMPARE (registry.provider (QStringLiteral ("minimax")).consoleUrl,
            QStringLiteral ("https://platform.minimaxi.com/user-center/billing"));
}

// ---------------------------------------------------------------------------
// Credential readers
// ---------------------------------------------------------------------------

void
ProviderRegistryTest::codexCredentialsReaderHandlesModes ()
{
  QTemporaryDir dir;
  const QString chatgpt = dir.filePath (QStringLiteral ("chatgpt.json"));
  writeFile (chatgpt, R"({
    "OPENAI_API_KEY": null,
    "auth_mode": "chatgpt",
    "tokens": {
      "access_token": "at", "refresh_token": "rt",
      "id_token": "it", "account_id": "acc-1"
    }
  })");
  const CodexCredentials good = CredentialStore::readCodexCredentials (chatgpt);
  QVERIFY (good.present);
  QVERIFY (!good.apiKeyMode);
  QCOMPARE (good.accessToken, QStringLiteral ("at"));
  QCOMPARE (good.refreshToken, QStringLiteral ("rt"));
  QCOMPARE (good.accountId, QStringLiteral ("acc-1"));

  const QString apikey = dir.filePath (QStringLiteral ("apikey.json"));
  writeFile (apikey, R"({"auth_mode": "apikey", "tokens": {"access_token": "x"}})");
  const CodexCredentials api = CredentialStore::readCodexCredentials (apikey);
  QVERIFY (api.present);
  QVERIFY (api.apiKeyMode);

  const CodexCredentials missing = CredentialStore::readCodexCredentials (
      dir.filePath (QStringLiteral ("none.json")));
  QVERIFY (!missing.present);
}

void
ProviderRegistryTest::kimiCredentialsReaderCoercesSeconds ()
{
  QTemporaryDir dir;
  const QString path = dir.filePath (QStringLiteral ("kimi.json"));
  writeFile (path, R"({
    "access_token": "kt",
    "expires_at": 1790782108,
    "refresh_token": "rt"
  })");
  const KimiCredentials credentials = CredentialStore::readKimiCredentials (path);
  QVERIFY (credentials.present);
  QCOMPARE (credentials.accessToken, QStringLiteral ("kt"));
  // Seconds become milliseconds.
  QCOMPARE (credentials.expiresAtMs, qint64 (1790782108000));

  writeFile (path, R"({"expires_at": 1790782108123})");
  QCOMPARE (CredentialStore::readKimiCredentials (path).expiresAtMs,
            qint64 (1790782108123));
}

void
ProviderRegistryTest::zcodeReaderPicksActiveCodingPlanKey ()
{
  QTemporaryDir dir;
  const QString config = dir.filePath (QStringLiteral ("config.json"));
  writeFile (config, R"({
    "provider": {
      "builtin:bigmodel": {"options": {"apiKey": "", "baseURL": "https://open.bigmodel.cn/api/anthropic"}},
      "builtin:bigmodel-coding-plan": {"options": {"apiKey": "bm-key", "baseURL": "https://open.bigmodel.cn/api/anthropic"}},
      "builtin:zai-coding-plan": {"options": {"apiKey": "zai-key", "baseURL": "https://api.z.ai/api/anthropic"}}
    }
  })");
  const QString setting = dir.filePath (QStringLiteral ("setting.json"));
  writeFile (setting, R"({
    "providerFamilyDomain": "bigmodel",
    "modelProviderFamilySelectedKeys": {
      "zai": "coding-plan:builtin:zai-coding-plan",
      "bigmodel": "coding-plan:builtin:bigmodel-coding-plan"
    }
  })");

  const GlmPlanCredentials credentials = CredentialStore::readGlmPlanFromZCode (
      config, setting);
  QVERIFY (credentials.present);
  QCOMPARE (credentials.apiKey, QStringLiteral ("bm-key"));
  QCOMPARE (credentials.rootUrl, QStringLiteral ("https://open.bigmodel.cn"));
}

void
ProviderRegistryTest::zcodeReaderFallsBackToAnyCodingPlanKey ()
{
  QTemporaryDir dir;
  const QString config = dir.filePath (QStringLiteral ("config.json"));
  writeFile (config, R"({
    "provider": {
      "builtin:zai": {"options": {"apiKey": "", "baseURL": "https://api.z.ai/api/anthropic"}},
      "builtin:zai-coding-plan": {"options": {"apiKey": "zai-key", "baseURL": "https://api.z.ai/api/anthropic"}}
    }
  })");

  const GlmPlanCredentials credentials = CredentialStore::readGlmPlanFromZCode (
      config);
  QVERIFY (credentials.present);
  QCOMPARE (credentials.apiKey, QStringLiteral ("zai-key"));
  QCOMPARE (credentials.rootUrl, QStringLiteral ("https://api.z.ai"));

  writeFile (config, R"({"provider": {"builtin:zai-coding-plan": {"options": {"apiKey": "", "baseURL": "https://api.z.ai"}}}})");
  QVERIFY (!CredentialStore::readGlmPlanFromZCode (config).present);
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

void
ProviderRegistryTest::codexParserMapsWindowsAndPlan ()
{
  const QJsonObject root = parseJson (R"({
    "plan_type": "plus",
    "credits": {"has_credits": true, "unlimited": false, "balance": "12.5"},
    "rate_limit": {
      "primary_window": {"used_percent": 42.0, "limit_window_seconds": 18000,
                         "reset_at": 1790000000, "reset_after_seconds": 3600},
      "secondary_window": {"used_percent": 71.0, "limit_window_seconds": 604800,
                           "reset_at": 1790500000, "reset_after_seconds": 86400}
    }
  })");

  QuotaSnapshot templateSnapshot;
  templateSnapshot.providerId = QStringLiteral ("codex");
  templateSnapshot.providerName = QStringLiteral ("Codex / ChatGPT");

  const QuotaSnapshot snapshot = QuotaParsers::parseCodexUsage (root, templateSnapshot);
  QCOMPARE (snapshot.status, SnapshotStatus::Ok);
  QCOMPARE (snapshot.fiveHourRemainingRatio, 0.58); // 1 - 42%
  QCOMPARE (snapshot.remainingRatio, 0.29);         // 1 - 71%
  QVERIFY (snapshot.message.contains (QStringLiteral ("plus")));
  QVERIFY (snapshot.message.contains (QStringLiteral ("12.5")));
  QCOMPARE (snapshot.resetAt.toSecsSinceEpoch (), qint64 (1790000000));
}

void
ProviderRegistryTest::codexParserSwapsWindowRolesByDuration ()
{
  // Weekly reported first, session second — roles must follow the durations.
  const QJsonObject root = parseJson (R"({
    "plan_type": "pro",
    "rate_limit": {
      "primary_window": {"used_percent": 60.0, "limit_window_seconds": 604800, "reset_at": 1790500000},
      "secondary_window": {"used_percent": 10.0, "limit_window_seconds": 18000, "reset_at": 1790000000}
    }
  })");

  QuotaSnapshot templateSnapshot;
  const QuotaSnapshot snapshot = QuotaParsers::parseCodexUsage (root, templateSnapshot);
  QCOMPARE (snapshot.remainingRatio, 0.4);
  QCOMPARE (snapshot.fiveHourRemainingRatio, 0.9);
}

void
ProviderRegistryTest::codexParserRejectsEmptyRateLimit ()
{
  const QuotaSnapshot snapshot = QuotaParsers::parseCodexUsage (
      QJsonObject (), QuotaSnapshot ());
  QCOMPARE (snapshot.status, SnapshotStatus::ParseError);
}

void
ProviderRegistryTest::codexParserClassifiesLoneWindowByLength ()
{
  // A free plan reports only its weekly limit, as the primary window.
  const QuotaSnapshot weeklyOnly = QuotaParsers::parseCodexUsage (
      parseJson (R"({"rate_limit": {"primary_window": {"used_percent": 60.0, "limit_window_seconds": 604800, "reset_at": 1790500000}}})"),
      QuotaSnapshot ());
  QCOMPARE (weeklyOnly.status, SnapshotStatus::Ok);
  QCOMPARE (weeklyOnly.remainingRatio, 0.40);
  QVERIFY (weeklyOnly.fiveHourRemainingRatio < 0);

  // A lone 5-hour window in the secondary slot is still the session ring.
  const QuotaSnapshot sessionOnly = QuotaParsers::parseCodexUsage (
      parseJson (R"({"rate_limit": {"secondary_window": {"used_percent": 25.0, "limit_window_seconds": 18000}}})"),
      QuotaSnapshot ());
  QCOMPARE (sessionOnly.fiveHourRemainingRatio, 0.75);
  QVERIFY (sessionOnly.remainingRatio < 0);
}

void
ProviderRegistryTest::kimiParserMapsWeeklyAndSession ()
{
  const QJsonObject root = parseJson (R"({
    "usage": {"limit": "100", "used": "12", "resetTime": "2026-09-30T05:24:18.44Z"},
    "limits": [
      {"window": {"duration": 300, "timeUnit": "TIME_UNIT_MINUTE"},
       "detail": {"limit": "100", "remaining": "88", "resetTime": "2026-09-29T05:24:18.44Z"}}
    ]
  })");

  QuotaSnapshot templateSnapshot;
  const QuotaSnapshot snapshot = QuotaParsers::parseKimiUsages (root, templateSnapshot);
  QCOMPARE (snapshot.status, SnapshotStatus::Ok);
  // Weekly: 12/100 used.
  QCOMPARE (snapshot.remainingRatio, 0.88);
  QCOMPARE (snapshot.used, 12.0);
  QCOMPARE (snapshot.total, 100.0);
  // Session window: 88 remaining of 100.
  QCOMPARE (snapshot.fiveHourRemainingRatio, 0.88);
}

void
ProviderRegistryTest::kimiParserHandlesStringNumbers ()
{
  const QJsonObject root = parseJson (R"({
    "usage": {"limit": 200, "remaining": 50}
  })");

  const QuotaSnapshot snapshot = QuotaParsers::parseKimiUsages (root, QuotaSnapshot ());
  QCOMPARE (snapshot.status, SnapshotStatus::Ok);
  QCOMPARE (snapshot.remainingRatio, 0.25); // (200-50)/200 used
}

void
ProviderRegistryTest::glmParserMapsUnitWindows ()
{
  const QJsonObject root = parseJson (R"({
    "success": true,
    "data": {
      "level": "pro",
      "limits": [
        {"type": "TOKENS_LIMIT", "unit": 3, "number": 5, "percentage": 12,
         "usage": 100, "currentValue": 12, "nextResetTime": 1758800000000},
        {"type": "TOKENS_LIMIT", "unit": 6, "number": 1, "percentage": 40,
         "usage": 100, "currentValue": 40, "nextResetTime": 1759400000000},
        {"type": "TIME_LIMIT", "unit": 5, "number": 1, "percentage": 3}
      ]
    }
  })");

  QuotaSnapshot templateSnapshot;
  const QuotaSnapshot snapshot = QuotaParsers::parseGlmQuotaLimit (root, templateSnapshot);
  QCOMPARE (snapshot.status, SnapshotStatus::Ok);
  // 5-hour window (unit 3 × 5 hours = 300 min): 12% used.
  QCOMPARE (snapshot.fiveHourRemainingRatio, 0.88);
  // Weekly window (unit 6): 40% used.
  QCOMPARE (snapshot.remainingRatio, 0.60);
  QCOMPARE (snapshot.used, 40.0);
  QCOMPARE (snapshot.total, 100.0);
  QCOMPARE (snapshot.unit, QStringLiteral ("次"));
  QVERIFY (snapshot.message.contains (QStringLiteral ("pro")));
  QCOMPARE (snapshot.resetAt.toMSecsSinceEpoch (), qint64 (1758800000000));
}

void
ProviderRegistryTest::glmParserRejectsFailedReply ()
{
  // Zhipu answers 200 with success:false for a bad key — that is an auth
  // failure, not a parse problem.
  const QJsonObject authRoot = parseJson (
      R"({"success": false, "code": 401, "msg": "token expired or incorrect"})");
  const QuotaSnapshot authSnapshot = QuotaParsers::parseGlmQuotaLimit (
      authRoot, QuotaSnapshot ());
  QCOMPARE (authSnapshot.status, SnapshotStatus::AuthError);

  const QJsonObject otherRoot = parseJson (
      R"({"success": false, "code": 500, "msg": "server busy"})");
  const QuotaSnapshot otherSnapshot = QuotaParsers::parseGlmQuotaLimit (
      otherRoot, QuotaSnapshot ());
  QCOMPARE (otherSnapshot.status, SnapshotStatus::ParseError);
  QVERIFY (otherSnapshot.message.contains (QStringLiteral ("server busy")));
}

void
ProviderRegistryTest::glmParserClassifiesLoneWindowByLength ()
{
  const QuotaSnapshot sessionOnly = QuotaParsers::parseGlmQuotaLimit (
      parseJson (R"({"success": true, "data": {"limits": [{"type": "TOKENS_LIMIT", "unit": 3, "number": 5, "percentage": 30}]}})"),
      QuotaSnapshot ());
  QCOMPARE (sessionOnly.status, SnapshotStatus::Ok);
  QCOMPARE (sessionOnly.fiveHourRemainingRatio, 0.70);
  QVERIFY (sessionOnly.remainingRatio < 0);

  const QuotaSnapshot weeklyOnly = QuotaParsers::parseGlmQuotaLimit (
      parseJson (R"({"success": true, "data": {"limits": [{"type": "TOKENS_LIMIT", "unit": 6, "number": 1, "percentage": 20}]}})"),
      QuotaSnapshot ());
  QCOMPARE (weeklyOnly.remainingRatio, 0.80);
  QVERIFY (weeklyOnly.fiveHourRemainingRatio < 0);
}

void
ProviderRegistryTest::minimaxParserMapsGeneralBucket ()
{
  const QJsonObject root = parseJson (R"({
    "model_remains": [
      {"model_name": "general",
       "start_time": 1790000000, "end_time": 1790018000,
       "current_interval_remaining_percent": 25, "current_interval_status": 1,
       "current_interval_total_count": 0,
       "weekly_start_time": 1789900000, "weekly_end_time": 1790500000,
       "current_weekly_remaining_percent": 60, "current_weekly_status": 1},
      {"model_name": "video",
       "current_interval_remaining_percent": 90, "current_interval_status": 1,
       "current_weekly_remaining_percent": 95, "current_weekly_status": 1}
    ],
    "base_resp": {"status_code": 0, "status_msg": "success"}
  })");

  QuotaSnapshot templateSnapshot;
  const QuotaSnapshot snapshot = QuotaParsers::parseMinimaxRemains (root, templateSnapshot);
  QCOMPARE (snapshot.status, SnapshotStatus::Ok);
  // Session: 25% remaining; weekly: 60% remaining — read from "general".
  QCOMPARE (snapshot.fiveHourRemainingRatio, 0.25);
  QCOMPARE (snapshot.remainingRatio, 0.60);
  QCOMPARE (snapshot.resetAt.toSecsSinceEpoch (), qint64 (1790018000));
}

void
ProviderRegistryTest::minimaxParserTreats1004AsAuthError ()
{
  const QJsonObject root = parseJson (R"({
    "model_remains": [],
    "base_resp": {"status_code": 1004, "status_msg": "invalid api key"}
  })");
  const QuotaSnapshot snapshot = QuotaParsers::parseMinimaxRemains (root, QuotaSnapshot ());
  QCOMPARE (snapshot.status, SnapshotStatus::AuthError);
  QVERIFY (!snapshot.message.isEmpty ());
}

void
ProviderRegistryTest::minimaxParserRejectsMissingPercentages ()
{
  // Metered windows without their percentage are unknown, not "100% left".
  const QuotaSnapshot countsOnly = QuotaParsers::parseMinimaxRemains (
      parseJson (R"({"model_remains": [{"model_name": "general", "current_interval_total_count": 1500, "current_interval_usage_count": 300}], "base_resp": {"status_code": 0}})"),
      QuotaSnapshot ());
  QCOMPARE (countsOnly.status, SnapshotStatus::ParseError);
  QVERIFY (countsOnly.remainingRatio < 0);
  QVERIFY (countsOnly.fiveHourRemainingRatio < 0);

  const QuotaSnapshot unlimited = QuotaParsers::parseMinimaxRemains (
      parseJson (R"({"model_remains": [{"model_name": "general", "current_interval_status": 3, "current_weekly_status": 3}], "base_resp": {"status_code": 0}})"),
      QuotaSnapshot ());
  QCOMPARE (unlimited.status, SnapshotStatus::Ok);
  QCOMPARE (unlimited.message, QStringLiteral ("不限量"));
}

void
ProviderRegistryTest::epochHelperAdaptsSecondsAndMilliseconds ()
{
  QCOMPARE (QuotaParsers::epochToDateTime (1790000000).toSecsSinceEpoch (),
            qint64 (1790000000));
  QCOMPARE (QuotaParsers::epochToDateTime (1758800000000).toMSecsSinceEpoch (),
            qint64 (1758800000000));
  QVERIFY (!QuotaParsers::epochToDateTime (0).isValid ());
}

void
ProviderRegistryTest::usedPercentHelperClamps ()
{
  QCOMPARE (QuotaParsers::usedPercentToRemainingRatio (0.0), 1.0);
  QCOMPARE (QuotaParsers::usedPercentToRemainingRatio (100.0), 0.0);
  QCOMPARE (QuotaParsers::usedPercentToRemainingRatio (150.0), 0.0);
  QCOMPARE (QuotaParsers::usedPercentToRemainingRatio (-5.0), -1.0);
  QCOMPARE (QuotaParsers::usedPercentToRemainingRatio (42.0), 0.58);
}

void
ProviderRegistryTest::jwtExpiryDecodesPayload ()
{
  const QByteArray payload = QByteArrayLiteral ("{\"exp\":1893456000}");
  const QString jwt = QStringLiteral ("header.")
      + QString::fromLatin1 (payload.toBase64 (QByteArray::Base64UrlEncoding))
      + QStringLiteral (".signature");
  QCOMPARE (QuotaParsers::jwtExpirySeconds (jwt), qint64 (1893456000));
  QCOMPARE (QuotaParsers::jwtExpirySeconds (QStringLiteral ("junk")), qint64 (0));
}

// ---------------------------------------------------------------------------
// Codex token write-back
// ---------------------------------------------------------------------------

void
ProviderRegistryTest::codexTokenWriteBackPreservesFieldsAndPermissions ()
{
  QTemporaryDir dir;
  const QString path = dir.filePath (QStringLiteral ("auth.json"));
  writeFile (path, R"({
    "OPENAI_API_KEY": null,
    "auth_mode": "chatgpt",
    "last_refresh": "2026-09-01T00:00:00Z",
    "tokens": {
      "access_token": "old-at", "refresh_token": "old-rt",
      "id_token": "old-it", "account_id": "acc"
    }
  })");

  QVERIFY (DirectQuotaProvider::writeCodexTokensBack (
      path, QStringLiteral ("new-at"), QString (),
      QStringLiteral ("new-rt")));

  QFile authFile (path);
  QVERIFY (authFile.open (QIODevice::ReadOnly));
  const QJsonObject root = QJsonDocument::fromJson (authFile.readAll ()).object ();
  const QJsonObject tokens = root.value (QStringLiteral ("tokens")).toObject ();
  QCOMPARE (tokens.value (QStringLiteral ("access_token")).toString (),
            QStringLiteral ("new-at"));
  QCOMPARE (tokens.value (QStringLiteral ("refresh_token")).toString (),
            QStringLiteral ("new-rt"));
  // An empty id_token keeps the previous one; other fields survive untouched.
  QCOMPARE (tokens.value (QStringLiteral ("id_token")).toString (),
            QStringLiteral ("old-it"));
  QCOMPARE (tokens.value (QStringLiteral ("account_id")).toString (),
            QStringLiteral ("acc"));
  QCOMPARE (root.value (QStringLiteral ("auth_mode")).toString (),
            QStringLiteral ("chatgpt"));
  QVERIFY (!root.value (QStringLiteral ("last_refresh")).toString ().isEmpty ());

  const QFileDevice::Permissions permissions = QFileInfo (path).permissions ();
  // On Linux the owner bits are mirrored as user bits; group/other stay empty.
  QCOMPARE (permissions, QFileDevice::Permissions (
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser
      | QFileDevice::WriteUser));
}

// ---------------------------------------------------------------------------
// Model: entries, manual accounts, persistence
// ---------------------------------------------------------------------------

void
ProviderRegistryTest::autoEntriesFollowCredentialProbe ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex") || id == QStringLiteral ("glm-coding");
  });

  const QVariantList snapshots = model.snapshots ();
  QCOMPARE (snapshots.size (), 2);
  QCOMPARE (snapshots.at (0).toMap ().value (QStringLiteral ("entryId")).toString (),
            QStringLiteral ("codex"));
  QCOMPARE (snapshots.at (1).toMap ().value (QStringLiteral ("entryId")).toString (),
            QStringLiteral ("glm-coding"));

  model.setCredentialProbe ([](const QString &) { return false; });
  QCOMPARE (model.snapshots ().size (), 0);
  QVERIFY (!model.hasSubscriptions ());
}

void
ProviderRegistryTest::manualAccountRoundTrip ()
{
  resetPersistence ();
  QString entryId;
  {
    CodingPlanModel model;
    model.setCredentialProbe ([](const QString &) { return false; });
    model.addAccount (QStringLiteral ("minimax"), QStringLiteral ("工作号"),
                      QStringLiteral ("sk-cp-test"));

    const QVariantList snapshots = model.snapshots ();
    QCOMPARE (snapshots.size (), 1);
    const QVariantMap snapshot = snapshots.at (0).toMap ();
    entryId = snapshot.value (QStringLiteral ("entryId")).toString ();
    QVERIFY (entryId.startsWith (QStringLiteral ("manual:")));
    QCOMPARE (snapshot.value (QStringLiteral ("label")).toString (),
              QStringLiteral ("工作号"));
    QCOMPARE (snapshot.value (QStringLiteral ("source")).toString (),
              QStringLiteral ("manual"));
  }

  // A fresh model restores the manual account from persistence.
  {
    CodingPlanModel model;
    model.setCredentialProbe ([](const QString &) { return false; });
    const QVariantList snapshots = model.snapshots ();
    QCOMPARE (snapshots.size (), 1);
    QCOMPARE (snapshots.at (0).toMap ().value (QStringLiteral ("entryId")).toString (),
              entryId);

    model.removeAccount (entryId);
    QCOMPARE (model.snapshots ().size (), 0);
  }

  {
    CodingPlanModel model;
    model.setCredentialProbe ([](const QString &) { return false; });
    QCOMPARE (model.snapshots ().size (), 0);
  }
}

void
ProviderRegistryTest::manualAccountsSupportDuplicatesPerVendor ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &) { return false; });
  model.addAccount (QStringLiteral ("minimax"), QStringLiteral ("号1"),
                    QStringLiteral ("sk-one"));
  model.addAccount (QStringLiteral ("minimax"), QStringLiteral ("号2"),
                    QStringLiteral ("sk-two"));
  model.addAccount (QStringLiteral ("kimi-code"), QString (),
                    QStringLiteral ("kimi-key"));

  QCOMPARE (model.snapshots ().size (), 3);
  QStringList labels;
  for (const QVariant &value : model.snapshots ())
    {
      labels.append (value.toMap ().value (QStringLiteral ("label")).toString ());
    }
  QVERIFY (labels.contains (QStringLiteral ("号1")));
  QVERIFY (labels.contains (QStringLiteral ("号2")));
  QVERIFY (labels.contains (QStringLiteral ("Kimi Code"))); // default label
}

void
ProviderRegistryTest::providerKeyStoreIsOwnerOnly ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &) { return false; });
  model.addAccount (QStringLiteral ("minimax"), QStringLiteral ("perm"),
                    QStringLiteral ("sk-perm"));

  const QString keyStore = QStandardPaths::writableLocation (
      QStandardPaths::GenericConfigLocation)
      + QStringLiteral ("/dde-shell-coding-plan/provider-keys.json");
  QVERIFY (QFile::exists (keyStore));
  QCOMPARE (QFileInfo (keyStore).permissions (),
            QFileDevice::Permissions (
                QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ReadUser | QFileDevice::WriteUser));
}

void
ProviderRegistryTest::legacySnapshotsMigrateToAutoEntries ()
{
  resetPersistence ();
  // Extension-era persistence: no entryId, providerId doubles as identity.
  const QJsonArray legacy = [&]() {
    QJsonObject object;
    object.insert (QStringLiteral ("providerId"), QStringLiteral ("codex"));
    object.insert (QStringLiteral ("source"), QStringLiteral ("browser_ext"));
    object.insert (QStringLiteral ("status"), QStringLiteral ("ok"));
    object.insert (QStringLiteral ("remainingRatio"), 0.5);
    object.insert (QStringLiteral ("fiveHourRemainingRatio"), 0.25);
    QJsonArray array;
    array.append (object);
    return array;
  }();
  QSettings settings (QStringLiteral ("deepin"),
                      QStringLiteral ("dde-shell-coding-plan"));
  settings.setValue (QStringLiteral ("snapshots"),
                     QJsonDocument (legacy).toJson (QJsonDocument::Compact));
  settings.sync ();

  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });
  const QVariantList snapshots = model.snapshots ();
  QCOMPARE (snapshots.size (), 1);
  const QVariantMap snapshot = snapshots.at (0).toMap ();
  QCOMPARE (snapshot.value (QStringLiteral ("entryId")).toString (),
            QStringLiteral ("codex"));
  QCOMPARE (snapshot.value (QStringLiteral ("remainingRatio")).toDouble (), 0.5);
  QCOMPARE (snapshot.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (), 0.25);
}

void
ProviderRegistryTest::modelTooltipUsesFiveHourQuota ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });

  QVariantMap result;
  result.insert (QStringLiteral ("remainingRatio"), 0.75);
  result.insert (QStringLiteral ("fiveHourRemainingRatio"), 0.40);
  model.applyQuotaResult (QStringLiteral ("codex"), result);

  const QString tooltip = model.tooltipText ();
  QVERIFY (tooltip.contains (QStringLiteral ("Codex / ChatGPT: 40%")));
  QVERIFY (!tooltip.contains (QStringLiteral ("Codex / ChatGPT: 75%")));
}

void
ProviderRegistryTest::modelTooltipFallsBackToWeeklyQuota ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });

  QVariantMap result;
  result.insert (QStringLiteral ("remainingRatio"), 0.75);
  model.applyQuotaResult (QStringLiteral ("codex"), result);

  const QString tooltip = model.tooltipText ();
  QVERIFY (tooltip.contains (QStringLiteral ("Codex / ChatGPT: 75%")));
}

void
ProviderRegistryTest::refreshFailureKeepsRecentGoodSnapshot ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });

  QVariantMap result;
  result.insert (QStringLiteral ("remainingRatio"), 0.75);
  model.applyQuotaResult (QStringLiteral ("codex"), result);

  QVERIFY (QMetaObject::invokeMethod (&model, "onRefreshFailed", Qt::DirectConnection,
                                      Q_ARG (QString, QStringLiteral ("codex")),
                                      Q_ARG (QString, QStringLiteral ("网络中断")),
                                      Q_ARG (SnapshotStatus, SnapshotStatus::NetworkError)));

  const QVariantMap snapshot = model.snapshots ().at (0).toMap ();
  QCOMPARE (snapshot.value (QStringLiteral ("remainingRatio")).toDouble (), 0.75);
  QCOMPARE (snapshot.value (QStringLiteral ("message")).toString (),
            QStringLiteral ("网络中断"));
}

void
ProviderRegistryTest::refreshFailureReplacesStaleSnapshot ()
{
  resetPersistence ();
  {
    CodingPlanModel model;
    model.setCredentialProbe ([](const QString &id) {
      return id == QStringLiteral ("codex");
    });

    QVariantMap result;
    result.insert (QStringLiteral ("remainingRatio"), 0.75);
    model.applyQuotaResult (QStringLiteral ("codex"), result);
  }

  // Age the persisted snapshot beyond the 30-minute keep window, then let a
  // fresh model load it before the failure arrives.
  QSettings settings (QStringLiteral ("deepin"),
                      QStringLiteral ("dde-shell-coding-plan"));
  const QByteArray raw = settings.value (QStringLiteral ("snapshots")).toByteArray ();
  QJsonArray array = QJsonDocument::fromJson (raw).array ();
  QJsonObject object = array.at (0).toObject ();
  object.insert (QStringLiteral ("updatedAt"),
                 QDateTime::currentDateTimeUtc ().addSecs (-3600).toString (Qt::ISODate));
  array.replace (0, object);
  settings.setValue (QStringLiteral ("snapshots"),
                     QJsonDocument (array).toJson (QJsonDocument::Compact));
  settings.sync ();

  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });

  QVERIFY (QMetaObject::invokeMethod (&model, "onRefreshFailed", Qt::DirectConnection,
                                      Q_ARG (QString, QStringLiteral ("codex")),
                                      Q_ARG (QString, QStringLiteral ("认证失效")),
                                      Q_ARG (SnapshotStatus, SnapshotStatus::AuthError)));

  const QVariantMap snapshot = model.snapshots ().at (0).toMap ();
  QCOMPARE (snapshot.value (QStringLiteral ("status")).toString (),
            QStringLiteral ("auth_error"));
  QCOMPARE (snapshot.value (QStringLiteral ("message")).toString (),
            QStringLiteral ("认证失效"));
  QVERIFY (snapshot.value (QStringLiteral ("remainingRatio")).toDouble () < 0);
}

void
ProviderRegistryTest::refreshFailureKeepWindowDoesNotSlide ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });

  QVariantMap result;
  result.insert (QStringLiteral ("remainingRatio"), 0.75);
  model.applyQuotaResult (QStringLiteral ("codex"), result);
  const QString goodAt = model.snapshots ().at (0).toMap ()
                             .value (QStringLiteral ("updatedAt")).toString ();

  QTest::qWait (1100);
  QVERIFY (QMetaObject::invokeMethod (&model, "onRefreshFailed", Qt::DirectConnection,
                                      Q_ARG (QString, QStringLiteral ("codex")),
                                      Q_ARG (QString, QStringLiteral ("网络中断")),
                                      Q_ARG (SnapshotStatus, SnapshotStatus::NetworkError)));

  // The kept snapshot still dates from the last success, so repeated
  // failures cannot extend the 30-minute keep window forever.
  const QVariantMap snapshot = model.snapshots ().at (0).toMap ();
  QCOMPARE (snapshot.value (QStringLiteral ("remainingRatio")).toDouble (), 0.75);
  QCOMPARE (snapshot.value (QStringLiteral ("updatedAt")).toString (), goodAt);
}

void
ProviderRegistryTest::authFailureIsNeverMaskedByStaleSnapshot ()
{
  resetPersistence ();
  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });

  QVariantMap result;
  result.insert (QStringLiteral ("remainingRatio"), 0.75);
  model.applyQuotaResult (QStringLiteral ("codex"), result);

  QVERIFY (QMetaObject::invokeMethod (&model, "onRefreshFailed", Qt::DirectConnection,
                                      Q_ARG (QString, QStringLiteral ("codex")),
                                      Q_ARG (QString, QStringLiteral ("认证失效")),
                                      Q_ARG (SnapshotStatus, SnapshotStatus::AuthError)));

  const QVariantMap snapshot = model.snapshots ().at (0).toMap ();
  QCOMPARE (snapshot.value (QStringLiteral ("status")).toString (),
            QStringLiteral ("auth_error"));
  QVERIFY (snapshot.value (QStringLiteral ("remainingRatio")).toDouble () < 0);
}

void
ProviderRegistryTest::snapshotsPersistResetAndCounts ()
{
  resetPersistence ();
  QuotaSnapshot parsed;
  parsed.providerId = QStringLiteral ("codex");
  parsed.status = SnapshotStatus::Ok;
  parsed.remainingRatio = 0.6;
  parsed.used = 40;
  parsed.total = 100;
  parsed.unit = QStringLiteral ("次");
  parsed.resetAt = QDateTime::fromSecsSinceEpoch (1790500000, QTimeZone::UTC);
  parsed.updatedAt = QDateTime::currentDateTimeUtc ();
  {
    CodingPlanModel model;
    model.setCredentialProbe ([](const QString &id) {
      return id == QStringLiteral ("codex");
    });
    QVERIFY (QMetaObject::invokeMethod (&model, "onRefreshCompleted", Qt::DirectConnection,
                                        Q_ARG (QString, QStringLiteral ("codex")),
                                        Q_ARG (QuotaSnapshot, parsed)));
  }

  CodingPlanModel model;
  model.setCredentialProbe ([](const QString &id) {
    return id == QStringLiteral ("codex");
  });
  const QVariantMap snapshot = model.snapshots ().at (0).toMap ();
  QCOMPARE (snapshot.value (QStringLiteral ("used")).toDouble (), 40.0);
  QCOMPARE (snapshot.value (QStringLiteral ("total")).toDouble (), 100.0);
  QCOMPARE (snapshot.value (QStringLiteral ("unit")).toString (), QStringLiteral ("次"));
  QCOMPARE (QDateTime::fromString (snapshot.value (QStringLiteral ("resetAt")).toString (),
                                   Qt::ISODate).toSecsSinceEpoch (),
            qint64 (1790500000));
}

void
ProviderRegistryTest::modelAutoRefreshIsCppSide ()
{
  const QString header = readFile (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY (header.contains (QStringLiteral ("kAutoRefreshIntervalMs"))
           || readFile (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.cpp"))
                   .contains (QStringLiteral ("kAutoRefreshIntervalMs")));
  QVERIFY (header.contains (QStringLiteral ("startAutoRefresh")));
}

// ---------------------------------------------------------------------------
// Panel / build structure contracts
// ---------------------------------------------------------------------------

void
ProviderRegistryTest::panelQmlLeftClickOpensStatusPopup ()
{
  const QString qml = readFile (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY (qml.contains (QStringLiteral ("TapHandler")));
  QVERIFY (qml.contains (QStringLiteral ("acceptedButtons: Qt.LeftButton")));
  QVERIFY (qml.contains (QStringLiteral ("popup.open()")));
  QVERIFY (!qml.contains (QStringLiteral ("import QtWebEngine")));
  QVERIFY (!qml.contains (QStringLiteral ("WebEngineView")));
}

void
ProviderRegistryTest::panelQmlHasAccountManagement ()
{
  const QString qml = readFile (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY (qml.contains (QStringLiteral ("manualCapableProviders")));
  QVERIFY (qml.contains (QStringLiteral ("addAccount")));
  QVERIFY (qml.contains (QStringLiteral ("removeAccount")));
  QVERIFY (qml.contains (QStringLiteral ("modelData.entryId")));
  QVERIFY (qml.contains (QStringLiteral ("刷新全部")));
  QVERIFY (qml.contains (QStringLiteral ("未检测到已登录的 Coding CLI")));
}

void
ProviderRegistryTest::panelQmlHasNoExtensionPairingUi ()
{
  const QString qml = readFile (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY (!qml.contains (QStringLiteral ("extensionConnected")));
  QVERIFY (!qml.contains (QStringLiteral ("extensionToken")));
  QVERIFY (!qml.contains (QStringLiteral ("配对")));
  QVERIFY (!qml.contains (QStringLiteral ("浏览器扩展")));

  const QString header = readFile (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY (!header.contains (QStringLiteral ("extensionConnected")));
  QVERIFY (!header.contains (QStringLiteral ("extensionToken")));
  QVERIFY (!header.contains (QStringLiteral ("setBrowserExtResult")));
  QVERIFY (!header.contains (QStringLiteral ("WebSocketServer")));
}

void
ProviderRegistryTest::panelQmlFiveHourFallsBackToText ()
{
  const QString qml = readFile (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY (qml.contains (QStringLiteral ("fiveHourRemainingRatio")));
  QVERIFY (qml.contains (QStringLiteral ("fiveHourBalanceText")));
  QVERIFY (qml.contains (QStringLiteral ("fiveHourQuotaPercent")));
}

void
ProviderRegistryTest::panelQmlDockWidgetUsesOneFiveHourRingPerProvider ()
{
  const QString qml = readFile (QStringLiteral (SOURCE_DIR "/package/main.qml"));
  QVERIFY (qml.contains (QStringLiteral ("visibleRingCount")));
  QVERIFY (qml.contains (QStringLiteral ("root.providerInitial(parent.snapshot.providerName)")));
}

void
ProviderRegistryTest::cmakeUsesQtNetworkWithoutWebSockets ()
{
  const QString cmake = readFile (QStringLiteral (SOURCE_DIR "/CMakeLists.txt"));
  QVERIFY (cmake.contains (QStringLiteral ("Qt6 REQUIRED COMPONENTS Core Gui Network")));
  QVERIFY (!cmake.contains (QStringLiteral ("WebSockets")));
  QVERIFY (!cmake.contains (QStringLiteral ("websocket_server")));
  QVERIFY (!cmake.contains (QStringLiteral ("browser_ext_provider")));
  QVERIFY (cmake.contains (QStringLiteral ("credential_store")));
  QVERIFY (cmake.contains (QStringLiteral ("quota_parsers")));
  QVERIFY (cmake.contains (QStringLiteral ("direct_quota_provider")));
  QVERIFY (!QFile::exists (QStringLiteral (SOURCE_DIR "/src/websocket_server.h")));
  QVERIFY (!QFile::exists (QStringLiteral (SOURCE_DIR "/src/browser_ext_provider.h")));
  QVERIFY (!QDir (QStringLiteral (SOURCE_DIR "/extension")).exists ());
}

void
ProviderRegistryTest::directProviderUsesOfficialEndpoints ()
{
  const QString source = readFile (
      QStringLiteral (SOURCE_DIR "/src/direct_quota_provider.cpp"));
  QVERIFY (source.contains (QStringLiteral (
      "https://chatgpt.com/backend-api/wham/usage")));
  QVERIFY (source.contains (QStringLiteral ("https://auth.openai.com/oauth/token")));
  QVERIFY (source.contains (QStringLiteral ("app_EMoamEEZ73f0CkXaXp7hrann")));
  QVERIFY (source.contains (QStringLiteral ("https://api.kimi.com/coding/v1/usages")));
  QVERIFY (source.contains (QStringLiteral ("/api/monitor/usage/quota/limit")));
  QVERIFY (source.contains (QStringLiteral (
      "https://api.minimaxi.com/v1/token_plan/remains")));
}

void
ProviderRegistryTest::directProviderSendsCodexCliIdentity ()
{
  const QString source = readFile (
      QStringLiteral (SOURCE_DIR "/src/direct_quota_provider.cpp"));
  QVERIFY (source.contains (QStringLiteral ("codex_cli_rs")));
  QVERIFY (source.contains (QStringLiteral ("chatgpt-account-id")));
  QVERIFY (source.contains (QStringLiteral ("OpenAI-Beta")));
  QVERIFY (source.contains (QStringLiteral ("originator")));
}

void
ProviderRegistryTest::kimiPolicyIsReadOnly ()
{
  const QString source = readFile (
      QStringLiteral (SOURCE_DIR "/src/direct_quota_provider.cpp"));
  // The Kimi CLI owns token refresh; the applet only asks the user to rerun it.
  QVERIFY (source.contains (QStringLiteral ("请运行一次 kimi CLI 刷新登录")));
  const QString kimiFetch = source.mid (source.indexOf (QStringLiteral ("fetchKimi")),
                                        source.indexOf (QStringLiteral ("fetchGlm"))
                                            - source.indexOf (QStringLiteral ("fetchKimi")));
  QVERIFY (!kimiFetch.contains (QStringLiteral ("write")));
}

void
ProviderRegistryTest::directProviderFailsOverToFullGlmUrl ()
{
  const QString source = readFile (
      QStringLiteral (SOURCE_DIR "/src/direct_quota_provider.cpp"));
  QVERIFY (source.contains (QStringLiteral ("kGlmFailoverRoot = \"https://api.z.ai\"")));
  // The failover keeps the quota path instead of hitting the bare host.
  QVERIFY (source.contains (QStringLiteral ("failoverRoot + QString::fromLatin1 (kGlmQuotaPath)")));
  // Timeouts count as unreachable and may fail over too.
  QVERIFY (source.contains (QStringLiteral ("if (timedOut")));
}

void
ProviderRegistryTest::directProviderGuardsCodexTokenRotation ()
{
  const QString source = readFile (
      QStringLiteral (SOURCE_DIR "/src/direct_quota_provider.cpp"));
  QVERIFY (source.contains (QStringLiteral ("if (!writeCodexTokensBack (")));
  QVERIFY (source.contains (QStringLiteral ("authInfo.isWritable ()")));
  // A rejected refresh token (400 invalid_grant) is an auth failure.
  QVERIFY (source.contains (QStringLiteral ("oauthRefresh && httpStatus >= 400")));

  const QString header = readFile (
      QStringLiteral (SOURCE_DIR "/src/direct_quota_provider.h"));
  QVERIFY (header.contains (QStringLiteral ("m_inFlight")));
}

void
ProviderRegistryTest::directProviderClearsInFlightOnSyncFailure ()
{
  DirectQuotaProvider provider;
  QSignalSpy failed (&provider, &DirectQuotaProvider::refreshFailed);
  QuotaRequest request;
  request.entryId = QStringLiteral ("manual:test");
  request.providerId = QStringLiteral ("minimax");
  request.manual = true;
  // No key: fails synchronously, and must not leave the entry marked pending.
  provider.refreshEntry (request);
  provider.refreshEntry (request);
  QCOMPARE (failed.count (), 2);
}

void
ProviderRegistryTest::modelHasNoSelfTriggeredSettingsWatcher ()
{
  // Only the model writes its settings now; watching them would reload on
  // every own write.
  const QString header = readFile (QStringLiteral (SOURCE_DIR "/src/codingplanmodel.h"));
  QVERIFY (!header.contains (QStringLiteral ("QFileSystemWatcher")));
  QVERIFY (!header.contains (QStringLiteral ("watchExternalChanges")));
}

void
ProviderRegistryTest::appletStartsAutoRefresh ()
{
  const QString source = readFile (
      QStringLiteral (SOURCE_DIR "/src/codingplanapplet.cpp"));
  QVERIFY (source.contains (QStringLiteral ("startAutoRefresh")));
  QVERIFY (!source.contains (QStringLiteral ("WebSocketServer")));
}

QTEST_MAIN (ProviderRegistryTest)
#include "providerregistry_test.moc"
