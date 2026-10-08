// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "providerregistry.h"

#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QSet>
#include <QString>

#include <functional>

// One quota account the panel tracks: an auto-detected CLI sign-in
// (entryId == providerId) or a manually added API key (entryId manual:<id>).
struct QuotaRequest
{
  QString entryId;
  QString providerId;
  bool manual = false;
  QString apiKey; // manual entries only
  QString label;
};

// Fetches coding-plan quotas straight from the vendors' official usage
// endpoints, using credentials read from the local coding-agent CLI config
// files (CredentialStore) or manually added API keys — no browser involved.
// All requests are asynchronous; each resolves into exactly one of the two
// signals. A refresh of an entry that is still pending is ignored. The Codex OAuth token is refreshed and written back to auth.json
// when it is about to expire (the one write we ever do to CLI files).
class DirectQuotaProvider : public QObject
{
  Q_OBJECT

public:
  explicit DirectQuotaProvider (QObject *parent = nullptr);

  void refreshEntry (const QuotaRequest &request);
  void refreshAll (const QList<QuotaRequest> &requests);

  // Providers whose local CLI credentials can be auto-detected.
  static QStringList detectableProviderIds ();
  static bool credentialsPresent (const QString &providerId);

  // Writes refreshed Codex tokens back into auth.json, keeping every other
  // field as the CLI wrote it. Public for tests.
  static bool writeCodexTokensBack (const QString &authPath,
                                    const QString &accessToken,
                                    const QString &idToken,
                                    const QString &refreshToken);

signals:
  void refreshCompleted (const QString &entryId, const QuotaSnapshot &snapshot);
  void refreshFailed (const QString &entryId, const QString &message,
                      SnapshotStatus status);

private:
  using JsonHandler = std::function<void (const QuotaRequest &, int httpStatus,
                                          const QJsonObject &)>;
  enum class Verb
  {
    Get,
    Post,
  };

  QuotaSnapshot snapshotTemplate (const QuotaRequest &request) const;
  void finish (const QuotaRequest &request, const QuotaSnapshot &snapshot);
  void execute (const QuotaRequest &request,
                const QNetworkRequest &networkRequest, Verb verb,
                const QByteArray &body, const QString &failoverUrl,
                const JsonHandler &handler);
  void fetchCodex (const QuotaRequest &request);
  void fetchClaude (const QuotaRequest &request);
  void fetchKimi (const QuotaRequest &request);
  void fetchGlm (const QuotaRequest &request, const QString &key,
                 const QString &rootUrl, const QString &failoverRoot);
  void fetchMinimax (const QuotaRequest &request);

  ProviderRegistry m_registry;
  QNetworkAccessManager m_nam;
  QSet<QString> m_inFlight; // entryIds with a request pending
};
