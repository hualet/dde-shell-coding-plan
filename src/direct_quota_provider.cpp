// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "direct_quota_provider.h"
#include "credential_store.h"
#include "quota_parsers.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>

namespace
{
constexpr int kRequestTimeoutMs = 30000;

// Codex identifies itself the way the CLI does; the backend serves and gates
// by that identity (see magpie's codexSign).
constexpr auto kCodexUsageUrl = "https://chatgpt.com/backend-api/wham/usage";
constexpr auto kCodexTokenUrl = "https://auth.openai.com/oauth/token";
constexpr auto kCodexClientId = "app_EMoamEEZ73f0CkXaXp7hrann";
constexpr auto kCodexVersion = "0.55.0";
constexpr auto kCodexUserAgent = "codex_cli_rs/0.55.0 (Linux x86_64) xterm-256color";

constexpr auto kKimiUsageUrl = "https://api.kimi.com/coding/v1/usages";
constexpr auto kMinimaxRemainsUrl = "https://api.minimaxi.com/v1/token_plan/remains";
constexpr auto kMinimaxRemainsUrlIntl = "https://api.minimax.io/v1/token_plan/remains";
constexpr auto kGlmQuotaPath = "/api/monitor/usage/quota/limit";
constexpr auto kGlmDefaultRoot = "https://open.bigmodel.cn";
constexpr auto kGlmFailoverRoot = "https://api.z.ai";

bool
isErrorStatus (SnapshotStatus status)
{
  switch (status)
    {
    case SnapshotStatus::AuthError:
    case SnapshotStatus::RateLimited:
    case SnapshotStatus::Unsupported:
    case SnapshotStatus::ParseError:
    case SnapshotStatus::NetworkError:
      return true;
    default:
      return false;
    }
}
}

DirectQuotaProvider::DirectQuotaProvider (QObject *parent)
    : QObject (parent), m_registry (ProviderRegistry::createDefault ())
{
  // Every request resolves into exactly one of the two signals; that is when
  // its entry may be fetched again.
  connect (this, &DirectQuotaProvider::refreshCompleted, this,
           [this](const QString &entryId) { m_inFlight.remove (entryId); });
  connect (this, &DirectQuotaProvider::refreshFailed, this,
           [this](const QString &entryId) { m_inFlight.remove (entryId); });
}

QStringList
DirectQuotaProvider::detectableProviderIds ()
{
  return { QStringLiteral ("codex"), QStringLiteral ("kimi-code"),
           QStringLiteral ("glm-coding") };
}

bool
DirectQuotaProvider::credentialsPresent (const QString &providerId)
{
  if (providerId == QStringLiteral ("codex"))
    {
      const CodexCredentials credentials = CredentialStore::readCodexCredentials (
          CredentialStore::codexAuthPath ());
      return credentials.present && !credentials.apiKeyMode;
    }
  if (providerId == QStringLiteral ("kimi-code"))
    {
      return CredentialStore::readKimiCredentials (
                 CredentialStore::kimiCredentialsPath ()).present;
    }
  if (providerId == QStringLiteral ("glm-coding"))
    {
      return CredentialStore::readGlmPlanFromZCode (
                 CredentialStore::zcodeConfigPath (),
                 CredentialStore::zcodeSettingPath ()).present;
    }
  return false;
}

bool
DirectQuotaProvider::writeCodexTokensBack (const QString &authPath,
                                           const QString &accessToken,
                                           const QString &idToken,
                                           const QString &refreshToken)
{
  QFile file (authPath);
  if (!file.open (QIODevice::ReadOnly))
    {
      return false;
    }
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson (file.readAll (), &error);
  file.close ();
  if (error.error != QJsonParseError::NoError || !document.isObject ())
    {
      return false;
    }

  // Keep every other field exactly as Codex CLI wrote it.
  QJsonObject root = document.object ();
  QJsonObject tokens = root.value (QStringLiteral ("tokens")).toObject ();
  tokens.insert (QStringLiteral ("access_token"), accessToken);
  if (!idToken.isEmpty ())
    {
      tokens.insert (QStringLiteral ("id_token"), idToken);
    }
  if (!refreshToken.isEmpty ())
    {
      tokens.insert (QStringLiteral ("refresh_token"), refreshToken);
    }
  root.insert (QStringLiteral ("tokens"), tokens);
  root.insert (QStringLiteral ("last_refresh"),
               QDateTime::currentDateTimeUtc ().toString (Qt::ISODateWithMs));

  QSaveFile output (authPath);
  if (!output.open (QIODevice::WriteOnly))
    {
      return false;
    }
  output.write (QJsonDocument (root).toJson (QJsonDocument::Indented));
  // QSaveFile ignores setPermissions; enforce owner-only after the commit.
  if (output.commit ())
    {
      return QFile::setPermissions (authPath,
                                    QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
  return false;
}

void
DirectQuotaProvider::refreshEntry (const QuotaRequest &request)
{
  // Overlapping refreshes would double-spend the rotating Codex refresh
  // token and race each other's snapshots; the pending one will answer.
  if (m_inFlight.contains (request.entryId))
    {
      return;
    }
  m_inFlight.insert (request.entryId);

  if (request.providerId == QStringLiteral ("codex"))
    {
      fetchCodex (request);
    }
  else if (request.providerId == QStringLiteral ("kimi-code"))
    {
      fetchKimi (request);
    }
  else if (request.providerId == QStringLiteral ("glm-coding"))
    {
      if (request.manual)
        {
          // A manual key belongs to one of the two sites; the other host is
          // only tried when the first is unreachable, never on an auth
          // rejection.
          fetchGlm (request, request.apiKey, QString::fromLatin1 (kGlmDefaultRoot),
                    QString::fromLatin1 (kGlmFailoverRoot));
          return;
        }
      const GlmPlanCredentials plan = CredentialStore::readGlmPlanFromZCode (
          CredentialStore::zcodeConfigPath (), CredentialStore::zcodeSettingPath ());
      if (!plan.present)
        {
          emit refreshFailed (request.entryId,
                              QStringLiteral ("未检测到 ZCode 的 Coding Plan 登录信息。"),
                              SnapshotStatus::AuthError);
          return;
        }
      fetchGlm (request, plan.apiKey, plan.rootUrl, QString ());
    }
  else if (request.providerId == QStringLiteral ("minimax"))
    {
      fetchMinimax (request);
    }
  else
    {
      emit refreshFailed (request.entryId,
                          QStringLiteral ("未知的提供商：%1").arg (request.providerId),
                          SnapshotStatus::Unsupported);
    }
}

void
DirectQuotaProvider::refreshAll (const QList<QuotaRequest> &requests)
{
  for (const QuotaRequest &request : requests)
    {
      refreshEntry (request);
    }
}

QuotaSnapshot
DirectQuotaProvider::snapshotTemplate (const QuotaRequest &request) const
{
  QuotaSnapshot snapshot;
  const ProviderDefinition provider = m_registry.provider (request.providerId);
  snapshot.providerId = request.providerId;
  snapshot.providerName = provider.name;
  snapshot.source = request.manual ? SourceType::Manual : SourceType::OfficialApi;
  snapshot.status = SnapshotStatus::Unsupported;
  snapshot.consoleUrl = provider.consoleUrl;
  return snapshot;
}

void
DirectQuotaProvider::finish (const QuotaRequest &request,
                             const QuotaSnapshot &snapshot)
{
  if (isErrorStatus (snapshot.status))
    {
      emit refreshFailed (request.entryId,
                          snapshot.message.isEmpty ()
                              ? QStringLiteral ("读取额度失败。")
                              : snapshot.message,
                          snapshot.status);
      return;
    }
  emit refreshCompleted (request.entryId, snapshot);
}

void
DirectQuotaProvider::execute (const QuotaRequest &request,
                              const QNetworkRequest &networkRequest, Verb verb,
                              const QByteArray &body, const QString &failoverUrl,
                              const JsonHandler &handler)
{
  QNetworkReply *reply = verb == Verb::Get
      ? m_nam.get (networkRequest)
      : m_nam.post (networkRequest, body);

  auto *timeoutTimer = new QTimer (reply);
  timeoutTimer->setSingleShot (true);
  connect (timeoutTimer, &QTimer::timeout, reply, &QNetworkReply::abort);
  timeoutTimer->start (kRequestTimeoutMs);

  connect (reply, &QNetworkReply::finished, this,
           [this, request, networkRequest, verb, body, failoverUrl, handler,
            reply]() {
             reply->deleteLater ();
             const int httpStatus = reply->attribute (
                 QNetworkRequest::HttpStatusCodeAttribute).toInt ();
             const QString providerName = snapshotTemplate (request).providerName;

             // A timeout (our abort) is as much "unreachable" as a
             // connection error: both may fail over to the other host.
             const bool timedOut
                 = reply->error () == QNetworkReply::OperationCanceledError;
             if (timedOut
                 || (reply->error () != QNetworkReply::NoError && httpStatus == 0))
               {
                 if (!failoverUrl.isEmpty ())
                   {
                     QNetworkRequest failoverRequest = networkRequest;
                     failoverRequest.setUrl (QUrl (failoverUrl));
                     execute (request, failoverRequest, verb, body, QString (),
                              handler);
                     return;
                   }
                 emit refreshFailed (request.entryId,
                                     timedOut
                                         ? QStringLiteral ("%1 请求超时。").arg (providerName)
                                         : QStringLiteral ("%1 网络错误：%2")
                                               .arg (providerName, reply->errorString ()),
                                     SnapshotStatus::NetworkError);
                 return;
               }

             // The only POST is the Codex OAuth refresh, which rejects a
             // revoked or reused refresh token with 400 invalid_grant.
             const bool oauthRefresh = verb == Verb::Post;
             if (oauthRefresh && httpStatus >= 400 && httpStatus < 500)
               {
                 emit refreshFailed (
                     request.entryId,
                     QStringLiteral ("Codex 登录已过期且刷新失败（HTTP %1），请运行 codex login。")
                         .arg (httpStatus),
                     SnapshotStatus::AuthError);
                 return;
               }

             if (httpStatus == 401 || httpStatus == 403)
               {
                 emit refreshFailed (request.entryId,
                                     QStringLiteral ("%1 认证失败（HTTP %2），凭据可能已失效。")
                                         .arg (providerName)
                                         .arg (httpStatus),
                                     SnapshotStatus::AuthError);
                 return;
               }

             if (httpStatus == 429)
               {
                 emit refreshFailed (request.entryId,
                                     QStringLiteral ("%1 请求过于频繁，稍后重试。").arg (providerName),
                                     SnapshotStatus::RateLimited);
                 return;
               }

             if (httpStatus >= 300)
               {
                 emit refreshFailed (request.entryId,
                                     QStringLiteral ("%1 接口返回 HTTP %2。")
                                         .arg (providerName)
                                         .arg (httpStatus),
                                     SnapshotStatus::NetworkError);
                 return;
               }

             QJsonParseError parseError;
             const QJsonDocument document = QJsonDocument::fromJson (
                 reply->readAll (), &parseError);
             if (parseError.error != QJsonParseError::NoError
                 || !document.isObject ())
               {
                 emit refreshFailed (request.entryId,
                                     QStringLiteral ("%1 响应不是有效的 JSON。").arg (providerName),
                                     SnapshotStatus::ParseError);
                 return;
               }

             handler (request, httpStatus, document.object ());
           });
}

void
DirectQuotaProvider::fetchCodex (const QuotaRequest &request)
{
  const CodexCredentials credentials = CredentialStore::readCodexCredentials (
      CredentialStore::codexAuthPath ());
  if (!credentials.present)
    {
      emit refreshFailed (request.entryId,
                          QStringLiteral ("未检测到 Codex 登录，请运行 codex login。"),
                          SnapshotStatus::AuthError);
      return;
    }
  if (credentials.apiKeyMode)
    {
      emit refreshFailed (
          request.entryId,
          QStringLiteral ("Codex 处于 API Key 模式，无法读取 ChatGPT 套餐额度。"),
          SnapshotStatus::Unsupported);
      return;
    }

  const auto requestUsage = [this, request, credentials](const QString &accessToken) {
    QNetworkRequest networkRequest (
        QUrl (QString::fromLatin1 (kCodexUsageUrl)));
    networkRequest.setRawHeader (QByteArrayLiteral ("Authorization"),
                                 (QStringLiteral ("Bearer ") + accessToken).toUtf8 ());
    if (!credentials.accountId.isEmpty ())
      {
        networkRequest.setRawHeader (QByteArrayLiteral ("chatgpt-account-id"),
                                     credentials.accountId.toUtf8 ());
      }
    networkRequest.setRawHeader (QByteArrayLiteral ("OpenAI-Beta"),
                                 QByteArrayLiteral ("responses=experimental"));
    networkRequest.setRawHeader (QByteArrayLiteral ("originator"),
                                 QByteArrayLiteral ("codex_cli_rs"));
    networkRequest.setRawHeader (QByteArrayLiteral ("version"),
                                 QByteArrayLiteral (kCodexVersion));
    networkRequest.setHeader (QNetworkRequest::UserAgentHeader,
                              QString::fromLatin1 (kCodexUserAgent));
    networkRequest.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

    execute (request, networkRequest, Verb::Get, QByteArray (), QString (),
             [this](const QuotaRequest &req, int, const QJsonObject &json) {
               finish (req, QuotaParsers::parseCodexUsage (json, snapshotTemplate (req)));
             });
  };

  const qint64 expiresAt = QuotaParsers::jwtExpirySeconds (credentials.accessToken);
  const bool needsRefresh = expiresAt > 0
      && expiresAt - QDateTime::currentSecsSinceEpoch () <= 300;
  if (!needsRefresh || credentials.refreshToken.isEmpty ())
    {
      requestUsage (credentials.accessToken);
      return;
    }

  // OpenAI rotates the refresh token: refreshing without being able to store
  // the new one would leave Codex CLI holding a dead token. Let the CLI do it.
  const QString authPath = CredentialStore::codexAuthPath ();
  const QFileInfo authInfo (authPath);
  if (!authInfo.isWritable ()
      || !QFileInfo (authInfo.absolutePath ()).isWritable ())
    {
      emit refreshFailed (
          request.entryId,
          QStringLiteral ("Codex 登录即将过期，且 auth.json 不可写，请运行一次 codex 刷新登录。"),
          SnapshotStatus::AuthError);
      return;
    }

  // Refresh through the CLI's own OAuth client, then write the rotated
  // tokens back so Codex CLI keeps working (magpie's approach).
  QNetworkRequest tokenRequest (QUrl (QString::fromLatin1 (kCodexTokenUrl)));
  tokenRequest.setHeader (QNetworkRequest::ContentTypeHeader,
                          QStringLiteral ("application/json"));
  tokenRequest.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                           QNetworkRequest::NoLessSafeRedirectPolicy);
  QJsonObject body;
  body.insert (QStringLiteral ("client_id"), QString::fromLatin1 (kCodexClientId));
  body.insert (QStringLiteral ("grant_type"), QStringLiteral ("refresh_token"));
  body.insert (QStringLiteral ("refresh_token"), credentials.refreshToken);
  body.insert (QStringLiteral ("scope"), QStringLiteral ("openid profile email"));

  execute (request, tokenRequest, Verb::Post,
           QJsonDocument (body).toJson (QJsonDocument::Compact), QString (),
           [this, request, requestUsage, authPath](const QuotaRequest &, int,
                                                   const QJsonObject &json) {
             const QString accessToken = json.value (
                 QStringLiteral ("access_token")).toString ();
             if (accessToken.isEmpty ())
               {
                 emit refreshFailed (
                     request.entryId,
                     QStringLiteral ("Codex 登录已过期且刷新失败，请运行 codex login。"),
                     SnapshotStatus::AuthError);
                 return;
               }
             if (!writeCodexTokensBack (
                     authPath, accessToken,
                     json.value (QStringLiteral ("id_token")).toString (),
                     json.value (QStringLiteral ("refresh_token")).toString ()))
               {
                 qWarning () << "[coding-plan] cannot store refreshed Codex tokens in"
                             << authPath;
                 emit refreshFailed (
                     request.entryId,
                     QStringLiteral ("Codex 新令牌写回 auth.json 失败，Codex CLI 可能需要重新运行 codex login。"),
                     SnapshotStatus::AuthError);
                 return;
               }
             requestUsage (accessToken);
           });
}

void
DirectQuotaProvider::fetchKimi (const QuotaRequest &request)
{
  QString token = request.apiKey;
  if (!request.manual)
    {
      const KimiCredentials credentials = CredentialStore::readKimiCredentials (
          CredentialStore::kimiCredentialsPath ());
      if (!credentials.present)
        {
          emit refreshFailed (request.entryId,
                              QStringLiteral ("未检测到 Kimi Code 登录，请先运行 kimi 登录。"),
                              SnapshotStatus::AuthError);
          return;
        }
      // Read-only policy: the Kimi CLI owns token refresh; a rotated refresh
      // token would kill its live session, so we only ask the user to rerun it.
      if (credentials.expiresAtMs > 0
          && credentials.expiresAtMs - QDateTime::currentMSecsSinceEpoch () <= 5000)
        {
          emit refreshFailed (request.entryId,
                              QStringLiteral ("Kimi 登录已过期，请运行一次 kimi CLI 刷新登录。"),
                              SnapshotStatus::AuthError);
          return;
        }
      token = credentials.accessToken;
    }

  if (token.trimmed ().isEmpty ())
    {
      emit refreshFailed (request.entryId, QStringLiteral ("未设置 Kimi API Key。"),
                          SnapshotStatus::AuthError);
      return;
    }

  QNetworkRequest networkRequest (QUrl (QString::fromLatin1 (kKimiUsageUrl)));
  networkRequest.setRawHeader (QByteArrayLiteral ("Authorization"),
                               (QStringLiteral ("Bearer ") + token).toUtf8 ());
  networkRequest.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

  execute (request, networkRequest, Verb::Get, QByteArray (), QString (),
           [this](const QuotaRequest &req, int, const QJsonObject &json) {
             finish (req, QuotaParsers::parseKimiUsages (json, snapshotTemplate (req)));
           });
}

void
DirectQuotaProvider::fetchGlm (const QuotaRequest &request, const QString &key,
                               const QString &rootUrl, const QString &failoverRoot)
{
  if (key.trimmed ().isEmpty ())
    {
      emit refreshFailed (request.entryId,
                          request.manual
                              ? QStringLiteral ("未设置 GLM API Key。")
                              : QStringLiteral ("未检测到 ZCode 的 Coding Plan Key。"),
                          SnapshotStatus::AuthError);
      return;
    }

  QNetworkRequest networkRequest (QUrl (rootUrl + QString::fromLatin1 (kGlmQuotaPath)));
  // Zhipu/Z.ai take the bare key as the Authorization value, no Bearer.
  networkRequest.setRawHeader (QByteArrayLiteral ("Authorization"), key.toUtf8 ());
  networkRequest.setRawHeader (QByteArrayLiteral ("Accept-Language"),
                               QByteArrayLiteral ("en-US,en"));
  networkRequest.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

  const QString failoverUrl = failoverRoot.isEmpty ()
      ? QString ()
      : failoverRoot + QString::fromLatin1 (kGlmQuotaPath);
  execute (request, networkRequest, Verb::Get, QByteArray (), failoverUrl,
           [this](const QuotaRequest &req, int, const QJsonObject &json) {
             finish (req, QuotaParsers::parseGlmQuotaLimit (json, snapshotTemplate (req)));
           });
}

void
DirectQuotaProvider::fetchMinimax (const QuotaRequest &request)
{
  if (request.apiKey.trimmed ().isEmpty ())
    {
      emit refreshFailed (request.entryId, QStringLiteral ("未设置 MiniMax API Key。"),
                          SnapshotStatus::AuthError);
      return;
    }

  QNetworkRequest networkRequest (QUrl (QString::fromLatin1 (kMinimaxRemainsUrl)));
  networkRequest.setRawHeader (QByteArrayLiteral ("Authorization"),
                               (QStringLiteral ("Bearer ") + request.apiKey).toUtf8 ());
  networkRequest.setAttribute (QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

  execute (request, networkRequest, Verb::Get, QByteArray (),
           QString::fromLatin1 (kMinimaxRemainsUrlIntl),
           [this](const QuotaRequest &req, int, const QJsonObject &json) {
             finish (req, QuotaParsers::parseMinimaxRemains (json, snapshotTemplate (req)));
           });
}
