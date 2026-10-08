// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "direct_quota_provider.h"
#include "providerregistry.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <functional>

// QML-facing model. Tracks quota accounts ("entries") from two sources:
// auto-detected coding-agent CLI sign-ins (Codex/Kimi/ZCode credential
// files) and manually added API keys. Each entry holds one QuotaSnapshot,
// persisted across restarts; DirectQuotaProvider fills them from the
// vendors' official usage endpoints.
class CodingPlanModel : public QObject
{
  Q_OBJECT
  Q_PROPERTY (QVariantList providers READ providers NOTIFY providersChanged)
  Q_PROPERTY (QVariantList manualCapableProviders READ manualCapableProviders NOTIFY providersChanged)
  Q_PROPERTY (QVariantList snapshots READ snapshots NOTIFY snapshotsChanged)
  Q_PROPERTY (bool hasSubscriptions READ hasSubscriptions NOTIFY snapshotsChanged)
  Q_PROPERTY (QString tooltipText READ tooltipText NOTIFY snapshotsChanged)

public:
  explicit CodingPlanModel (QObject *parent = nullptr);

  QVariantList providers () const;
  QVariantList manualCapableProviders () const;
  QVariantList snapshots () const;
  bool hasSubscriptions () const;
  QString tooltipText () const;

  Q_INVOKABLE void refreshAll ();
  Q_INVOKABLE void refreshProvider (const QString &providerId);
  Q_INVOKABLE void openConsole (const QString &providerId);
  // Manual accounts: provider must support key-based quota queries.
  Q_INVOKABLE void addAccount (const QString &providerId, const QString &label,
                               const QString &apiKey);
  Q_INVOKABLE void removeAccount (const QString &entryId);
  // Test seam: injects a parsed quota result for one entry.
  Q_INVOKABLE void applyQuotaResult (const QString &entryId,
                                     const QVariantMap &result);

  void startAutoRefresh ();

  // Test seam: decides which auto-detectable providers have credentials.
  void setCredentialProbe (const std::function<bool (const QString &)> &probe);

signals:
  void providersChanged ();
  void snapshotsChanged ();

private slots:
  void onRefreshCompleted (const QString &entryId, const QuotaSnapshot &snapshot);
  void onRefreshFailed (const QString &entryId, const QString &message,
                        SnapshotStatus status);

private:
  struct Entry
  {
    QString entryId;
    QString providerId;
    QString label;
    bool manual = false;
  };

  bool rebuildEntries ();
  bool entriesChanged (const QList<Entry> &candidates) const;
  int entryIndex (const QString &entryId) const;
  QuotaSnapshot placeholderSnapshot (const Entry &entry) const;
  void ensureSnapshots ();
  void loadPersistedState ();
  void saveSnapshots () const;
  void saveManualAccounts () const;
  QList<QuotaRequest> entryRequests () const;
  QuotaRequest requestForEntry (const Entry &entry,
                                const QHash<QString, QString> &apiKeys) const;

  static QString apiKeyFilePath ();
  QHash<QString, QString> loadApiKeys () const;
  void saveApiKeys (const QHash<QString, QString> &keys) const;
  void storeApiKey (const QString &entryId, const QString &apiKey) const;
  void removeApiKey (const QString &entryId) const;

  ProviderRegistry m_registry;
  QList<Entry> m_entries;
  QList<Entry> m_manualEntries;
  QHash<QString, QuotaSnapshot> m_snapshots;
  QTimer m_autoRefreshTimer;
  DirectQuotaProvider *m_provider = nullptr;
  std::function<bool (const QString &)> m_credentialProbe;
};
