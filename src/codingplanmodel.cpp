// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "codingplanmodel.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace
{
constexpr auto kSettingsOrganization = "deepin";
constexpr auto kSettingsApplication = "dde-shell-coding-plan";
constexpr auto kSnapshotsKey = "snapshots";
constexpr auto kManualAccountsKey = "manualAccounts";
constexpr auto kProviderKeysFile = "provider-keys.json";
constexpr int kAutoRefreshIntervalMs = 15 * 60 * 1000;
constexpr int kStaleKeepSeconds = 30 * 60; // keep a good snapshot this long on errors

// Providers whose quota an API key alone can read (manual accounts).
const QStringList &
manualCapableIds ()
{
  static const QStringList ids = { QStringLiteral ("kimi-code"),
                                   QStringLiteral ("glm-coding"),
                                   QStringLiteral ("minimax") };
  return ids;
}

SnapshotStatus
statusFromString (const QString &status)
{
  if (status == QStringLiteral ("ok"))
    {
      return SnapshotStatus::Ok;
    }
  if (status == QStringLiteral ("warning"))
    {
      return SnapshotStatus::Warning;
    }
  if (status == QStringLiteral ("exhausted"))
    {
      return SnapshotStatus::Exhausted;
    }
  if (status == QStringLiteral ("auth_error"))
    {
      return SnapshotStatus::AuthError;
    }
  if (status == QStringLiteral ("authenticated"))
    {
      return SnapshotStatus::Authenticated;
    }
  if (status == QStringLiteral ("rate_limited"))
    {
      return SnapshotStatus::RateLimited;
    }
  if (status == QStringLiteral ("parse_error"))
    {
      return SnapshotStatus::ParseError;
    }
  if (status == QStringLiteral ("network_error"))
    {
      return SnapshotStatus::NetworkError;
    }
  return SnapshotStatus::Unsupported;
}

bool
hasGoodData (const QuotaSnapshot &snapshot)
{
  return snapshot.status == SnapshotStatus::Ok
      || snapshot.status == SnapshotStatus::Warning
      || snapshot.status == SnapshotStatus::Exhausted;
}
}

CodingPlanModel::CodingPlanModel (QObject *parent)
    : QObject (parent), m_registry (ProviderRegistry::createDefault ())
{
  m_credentialProbe = [](const QString &providerId) {
    return DirectQuotaProvider::credentialsPresent (providerId);
  };

  m_provider = new DirectQuotaProvider (this);
  connect (m_provider, &DirectQuotaProvider::refreshCompleted, this,
           &CodingPlanModel::onRefreshCompleted);
  connect (m_provider, &DirectQuotaProvider::refreshFailed, this,
           &CodingPlanModel::onRefreshFailed);

  loadPersistedState ();
  rebuildEntries ();
  ensureSnapshots ();

  m_autoRefreshTimer.setInterval (kAutoRefreshIntervalMs);
  connect (&m_autoRefreshTimer, &QTimer::timeout, this,
           &CodingPlanModel::refreshAll);
}

QVariantList
CodingPlanModel::providers () const
{
  QVariantList result;
  for (const ProviderDefinition &provider : m_registry.providers ())
    {
      QVariantMap item;
      item.insert (QStringLiteral ("id"), provider.id);
      item.insert (QStringLiteral ("name"), provider.name);
      item.insert (QStringLiteral ("source"), sourceTypeToString (provider.sourceType));
      item.insert (QStringLiteral ("loginUrl"), provider.loginUrl);
      item.insert (QStringLiteral ("consoleUrl"), provider.consoleUrl);
      result.append (item);
    }
  return result;
}

QVariantList
CodingPlanModel::manualCapableProviders () const
{
  QVariantList result;
  for (const QString &id : manualCapableIds ())
    {
      const ProviderDefinition provider = m_registry.provider (id);
      if (provider.id.isEmpty ())
        {
          continue;
        }
      QVariantMap item;
      item.insert (QStringLiteral ("id"), provider.id);
      item.insert (QStringLiteral ("name"), provider.name);
      result.append (item);
    }
  return result;
}

QVariantList
CodingPlanModel::snapshots () const
{
  QVariantList result;
  for (const Entry &entry : m_entries)
    {
      result.append (m_snapshots.value (entry.entryId,
                                        placeholderSnapshot (entry)).toVariantMap ());
    }
  return result;
}

bool
CodingPlanModel::hasSubscriptions () const
{
  return !m_entries.isEmpty ();
}

QString
CodingPlanModel::tooltipText () const
{
  QStringList lines;
  for (const Entry &entry : m_entries)
    {
      const QuotaSnapshot snapshot = m_snapshots.value (
          entry.entryId, placeholderSnapshot (entry));
      QString value = snapshot.message;
      if (snapshot.fiveHourRemainingRatio >= 0)
        {
          value = QStringLiteral ("%1%").arg (
              qRound (snapshot.fiveHourRemainingRatio * 100));
        }
      else if (!snapshot.fiveHourBalanceText.isEmpty ())
        {
          value = snapshot.fiveHourBalanceText;
        }
      else if (snapshot.remainingRatio >= 0)
        {
          value = QStringLiteral ("%1%").arg (
              qRound (snapshot.remainingRatio * 100));
        }
      else if (!snapshot.balanceText.isEmpty ())
        {
          value = snapshot.balanceText;
        }
      const QString name = entry.label.isEmpty ()
          ? snapshot.providerName
          : QStringLiteral ("%1（%2）").arg (snapshot.providerName, entry.label);
      lines.append (QStringLiteral ("%1: %2").arg (name, value));
    }

  return lines.join (QLatin1Char ('\n'));
}

void
CodingPlanModel::refreshAll ()
{
  // Re-detect CLI credentials so a login/logout shows up without restart.
  if (rebuildEntries ())
    {
      emit providersChanged ();
      emit snapshotsChanged ();
    }
  ensureSnapshots ();

  const QList<QuotaRequest> requests = entryRequests ();
  if (!requests.isEmpty ())
    {
      m_provider->refreshAll (requests);
    }
}

void
CodingPlanModel::refreshProvider (const QString &providerId)
{
  const QHash<QString, QString> apiKeys = loadApiKeys ();
  for (const Entry &entry : m_entries)
    {
      if (entry.providerId == providerId)
        {
          m_provider->refreshEntry (requestForEntry (entry, apiKeys));
        }
    }
}

void
CodingPlanModel::openConsole (const QString &providerId)
{
  if (!m_registry.contains (providerId))
    {
      return;
    }

  QDesktopServices::openUrl (QUrl (m_registry.provider (providerId).consoleUrl));
}

void
CodingPlanModel::addAccount (const QString &providerId, const QString &label,
                             const QString &apiKey)
{
  if (!manualCapableIds ().contains (providerId))
    {
      return;
    }
  const QString key = apiKey.trimmed ();
  if (key.isEmpty ())
    {
      return;
    }

  Entry entry;
  entry.entryId = QStringLiteral ("manual:")
      + QUuid::createUuid ().toString (QUuid::Id128);
  entry.providerId = providerId;
  entry.label = label.trimmed ().isEmpty ()
      ? m_registry.provider (providerId).name
      : label.trimmed ();
  entry.manual = true;

  m_manualEntries.append (entry);
  saveManualAccounts ();
  storeApiKey (entry.entryId, key);

  rebuildEntries ();
  ensureSnapshots ();
  QuotaSnapshot snapshot = placeholderSnapshot (entry);
  snapshot.status = SnapshotStatus::Authenticated;
  snapshot.message = QStringLiteral ("已添加，等待读取额度");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots.insert (entry.entryId, snapshot);
  saveSnapshots ();
  emit providersChanged ();
  emit snapshotsChanged ();

  m_provider->refreshEntry (requestForEntry (entry, loadApiKeys ()));
}

void
CodingPlanModel::removeAccount (const QString &entryId)
{
  bool removed = false;
  for (int index = 0; index < m_manualEntries.size (); ++index)
    {
      if (m_manualEntries.at (index).entryId == entryId)
        {
          m_manualEntries.removeAt (index);
          removed = true;
          break;
        }
    }
  if (!removed)
    {
      return;
    }

  saveManualAccounts ();
  removeApiKey (entryId);
  m_snapshots.remove (entryId);

  rebuildEntries ();
  ensureSnapshots ();
  saveSnapshots ();
  emit providersChanged ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::applyQuotaResult (const QString &entryId,
                                   const QVariantMap &result)
{
  const int index = entryIndex (entryId);
  if (index < 0)
    {
      return;
    }

  const Entry entry = m_entries.at (index);
  QuotaSnapshot snapshot = m_snapshots.value (entryId, placeholderSnapshot (entry));

  if (result.contains (QStringLiteral ("remainingRatio")))
    {
      bool ok = false;
      const double weeklyRatio = result.value (QStringLiteral ("remainingRatio")).toDouble (&ok);
      snapshot.remainingRatio = (ok && weeklyRatio >= 0)
          ? std::max (0.0, std::min (1.0, weeklyRatio))
          : -1.0;
    }
  else
    {
      snapshot.remainingRatio = -1.0;
    }
  snapshot.balanceText = result.value (QStringLiteral ("balanceText")).toString ();

  if (result.contains (QStringLiteral ("fiveHourRemainingRatio")))
    {
      bool ok = false;
      const double fiveHourRatio = result.value (
          QStringLiteral ("fiveHourRemainingRatio")).toDouble (&ok);
      snapshot.fiveHourRemainingRatio = (ok && fiveHourRatio >= 0)
          ? std::max (0.0, std::min (1.0, fiveHourRatio))
          : -1.0;
    }
  else
    {
      snapshot.fiveHourRemainingRatio = -1.0;
    }
  snapshot.fiveHourBalanceText = result.value (
      QStringLiteral ("fiveHourBalanceText")).toString ();

  snapshot.status = SnapshotStatus::Ok;
  snapshot.message = result.value (QStringLiteral ("message")).toString ();
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots.insert (entryId, snapshot);
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::startAutoRefresh ()
{
  refreshAll ();
  m_autoRefreshTimer.start ();
}

void
CodingPlanModel::setCredentialProbe (const std::function<bool (const QString &)> &probe)
{
  m_credentialProbe = probe;
  const bool changed = rebuildEntries ();
  ensureSnapshots ();
  if (changed)
    {
      emit providersChanged ();
      emit snapshotsChanged ();
    }
}

bool
CodingPlanModel::rebuildEntries ()
{
  QList<Entry> entries;
  const QStringList detectable = DirectQuotaProvider::detectableProviderIds ();
  for (const QString &id : m_registry.providerIds ())
    {
      if (detectable.contains (id) && m_credentialProbe (id))
        {
          Entry entry;
          entry.entryId = id;
          entry.providerId = id;
          entry.manual = false;
          entries.append (entry);
        }
    }
  entries.append (m_manualEntries);

  if (!entriesChanged (entries))
    {
      return false;
    }

  m_entries = entries;
  // Drop snapshots of entries that no longer exist.
  QStringList activeIds;
  for (const Entry &entry : entries)
    {
      activeIds.append (entry.entryId);
    }
  for (const QString &key : m_snapshots.keys ())
    {
      if (!activeIds.contains (key))
        {
          m_snapshots.remove (key);
        }
    }
  return true;
}

bool
CodingPlanModel::entriesChanged (const QList<Entry> &candidates) const
{
  if (candidates.size () != m_entries.size ())
    {
      return true;
    }
  for (int index = 0; index < candidates.size (); ++index)
    {
      if (candidates.at (index).entryId != m_entries.at (index).entryId)
        {
          return true;
        }
    }
  return false;
}

QuotaSnapshot
CodingPlanModel::placeholderSnapshot (const Entry &entry) const
{
  QuotaSnapshot snapshot;
  const ProviderDefinition provider = m_registry.provider (entry.providerId);
  snapshot.entryId = entry.entryId;
  snapshot.label = entry.label;
  snapshot.providerId = entry.providerId;
  snapshot.providerName = provider.name;
  snapshot.source = entry.manual ? SourceType::Manual : SourceType::OfficialApi;
  snapshot.status = SnapshotStatus::Authenticated;
  snapshot.message = QStringLiteral ("等待读取额度");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  snapshot.consoleUrl = provider.consoleUrl;
  return snapshot;
}

void
CodingPlanModel::ensureSnapshots ()
{
  for (const Entry &entry : m_entries)
    {
      if (!m_snapshots.contains (entry.entryId))
        {
          m_snapshots.insert (entry.entryId, placeholderSnapshot (entry));
        }
      else
        {
          QuotaSnapshot snapshot = m_snapshots.value (entry.entryId);
          const bool changed = snapshot.entryId != entry.entryId
              || snapshot.label != entry.label;
          if (changed)
            {
              snapshot.entryId = entry.entryId;
              snapshot.label = entry.label;
              m_snapshots.insert (entry.entryId, snapshot);
            }
        }
    }
}

void
CodingPlanModel::loadPersistedState ()
{
  QSettings settings (QString::fromLatin1 (kSettingsOrganization),
                      QString::fromLatin1 (kSettingsApplication));

  // Manual accounts.
  m_manualEntries.clear ();
  const QStringList ids = settings.value (QString::fromLatin1 (kManualAccountsKey)
                                              + "/entries")
                              .toStringList ();
  const QStringList providers = settings.value (
      QString::fromLatin1 (kManualAccountsKey) + "/providers").toStringList ();
  const QStringList labels = settings.value (
      QString::fromLatin1 (kManualAccountsKey) + "/labels").toStringList ();
  const QHash<QString, QString> apiKeys = loadApiKeys ();
  for (int index = 0; index < ids.size () && index < providers.size (); ++index)
    {
      if (!manualCapableIds ().contains (providers.at (index))
          || !apiKeys.contains (ids.at (index)))
        {
          continue;
        }
      Entry entry;
      entry.entryId = ids.at (index);
      entry.providerId = providers.at (index);
      entry.label = index < labels.size () ? labels.at (index) : QString ();
      entry.manual = true;
      m_manualEntries.append (entry);
    }

  // Snapshots; legacy items without entryId fall back to providerId, which
  // is the entryId of an auto-detected account.
  m_snapshots.clear ();
  const QByteArray raw = settings.value (QString::fromLatin1 (kSnapshotsKey)).toByteArray ();
  const QJsonDocument document = QJsonDocument::fromJson (raw);
  const QJsonArray storedSnapshots = document.array ();
  for (const QJsonValue &value : storedSnapshots)
    {
      const QJsonObject object = value.toObject ();
      QuotaSnapshot snapshot;
      snapshot.entryId = object.value (QStringLiteral ("entryId")).toString ();
      if (snapshot.entryId.isEmpty ())
        {
          snapshot.entryId = object.value (QStringLiteral ("providerId")).toString ();
        }
      snapshot.label = object.value (QStringLiteral ("label")).toString ();
      snapshot.providerId = object.value (QStringLiteral ("providerId")).toString ();
      if (snapshot.entryId.isEmpty () || snapshot.providerId.isEmpty ()
          || !m_registry.contains (snapshot.providerId))
        {
          continue;
        }
      snapshot.providerName = m_registry.provider (snapshot.providerId).name;
      const QString sourceStr = object.value (QStringLiteral ("source")).toString ();
      snapshot.source = sourceStr == QStringLiteral ("manual")
          ? SourceType::Manual
          : SourceType::OfficialApi;
      snapshot.status = statusFromString (object.value (QStringLiteral ("status")).toString ());
      snapshot.remainingRatio = object.value (QStringLiteral ("remainingRatio")).toDouble (-1.0);
      snapshot.used = object.value (QStringLiteral ("used")).toDouble (snapshot.used);
      snapshot.total = object.value (QStringLiteral ("total")).toDouble (snapshot.total);
      snapshot.unit = object.value (QStringLiteral ("unit")).toString ();
      snapshot.resetAt = QDateTime::fromString (
          object.value (QStringLiteral ("resetAt")).toString (), Qt::ISODate);
      snapshot.balanceText = object.value (QStringLiteral ("balanceText")).toString ();
      snapshot.fiveHourRemainingRatio = object.value (
          QStringLiteral ("fiveHourRemainingRatio")).toDouble (-1.0);
      snapshot.fiveHourBalanceText = object.value (
          QStringLiteral ("fiveHourBalanceText")).toString ();
      snapshot.message = object.value (QStringLiteral ("message")).toString ();
      snapshot.updatedAt = QDateTime::fromString (
          object.value (QStringLiteral ("updatedAt")).toString (), Qt::ISODate);
      snapshot.consoleUrl = m_registry.provider (snapshot.providerId).consoleUrl;
      m_snapshots.insert (snapshot.entryId, snapshot);
    }
}

void
CodingPlanModel::saveSnapshots () const
{
  QJsonArray array;
  for (const Entry &entry : m_entries)
    {
      const QuotaSnapshot snapshot = m_snapshots.value (entry.entryId);
      QJsonObject object;
      object.insert (QStringLiteral ("entryId"), snapshot.entryId);
      object.insert (QStringLiteral ("label"), snapshot.label);
      object.insert (QStringLiteral ("providerId"), snapshot.providerId);
      object.insert (QStringLiteral ("source"), sourceTypeToString (snapshot.source));
      object.insert (QStringLiteral ("status"), snapshotStatusToString (snapshot.status));
      object.insert (QStringLiteral ("remainingRatio"), snapshot.remainingRatio);
      object.insert (QStringLiteral ("used"), snapshot.used);
      object.insert (QStringLiteral ("total"), snapshot.total);
      object.insert (QStringLiteral ("unit"), snapshot.unit);
      object.insert (QStringLiteral ("resetAt"), snapshot.resetAt.toString (Qt::ISODate));
      object.insert (QStringLiteral ("balanceText"), snapshot.balanceText);
      object.insert (QStringLiteral ("fiveHourRemainingRatio"), snapshot.fiveHourRemainingRatio);
      object.insert (QStringLiteral ("fiveHourBalanceText"), snapshot.fiveHourBalanceText);
      object.insert (QStringLiteral ("message"), snapshot.message);
      object.insert (QStringLiteral ("updatedAt"), snapshot.updatedAt.toString (Qt::ISODate));
      array.append (object);
    }

  QSettings settings (QString::fromLatin1 (kSettingsOrganization),
                      QString::fromLatin1 (kSettingsApplication));
  settings.setValue (QString::fromLatin1 (kSnapshotsKey),
                     QJsonDocument (array).toJson (QJsonDocument::Compact));
  settings.sync ();
}

void
CodingPlanModel::saveManualAccounts () const
{
  QStringList ids;
  QStringList providers;
  QStringList labels;
  for (const Entry &entry : m_manualEntries)
    {
      ids.append (entry.entryId);
      providers.append (entry.providerId);
      labels.append (entry.label);
    }

  QSettings settings (QString::fromLatin1 (kSettingsOrganization),
                      QString::fromLatin1 (kSettingsApplication));
  settings.setValue (QString::fromLatin1 (kManualAccountsKey) + "/entries", ids);
  settings.setValue (QString::fromLatin1 (kManualAccountsKey) + "/providers", providers);
  settings.setValue (QString::fromLatin1 (kManualAccountsKey) + "/labels", labels);
  settings.sync ();
}

QList<QuotaRequest>
CodingPlanModel::entryRequests () const
{
  QList<QuotaRequest> requests;
  const QHash<QString, QString> apiKeys = loadApiKeys ();
  for (const Entry &entry : m_entries)
    {
      requests.append (requestForEntry (entry, apiKeys));
    }
  return requests;
}

QuotaRequest
CodingPlanModel::requestForEntry (const Entry &entry,
                                  const QHash<QString, QString> &apiKeys) const
{
  QuotaRequest request;
  request.entryId = entry.entryId;
  request.providerId = entry.providerId;
  request.manual = entry.manual;
  request.label = entry.label;
  if (entry.manual)
    {
      request.apiKey = apiKeys.value (entry.entryId);
    }
  return request;
}

int
CodingPlanModel::entryIndex (const QString &entryId) const
{
  for (int index = 0; index < m_entries.size (); ++index)
    {
      if (m_entries.at (index).entryId == entryId)
        {
          return index;
        }
    }

  return -1;
}

QString
CodingPlanModel::apiKeyFilePath ()
{
  const QString configDir = QStandardPaths::writableLocation (
      QStandardPaths::GenericConfigLocation)
      + QStringLiteral ("/dde-shell-coding-plan");
  return configDir + QStringLiteral ("/") + QString::fromLatin1 (kProviderKeysFile);
}

QHash<QString, QString>
CodingPlanModel::loadApiKeys () const
{
  QHash<QString, QString> keys;
  QFile file (apiKeyFilePath ());
  if (!file.open (QIODevice::ReadOnly))
    {
      return keys;
    }
  const QJsonDocument document = QJsonDocument::fromJson (file.readAll ());
  const QJsonObject object = document.object ();
  for (const QString &entryId : object.keys ())
    {
      keys.insert (entryId, object.value (entryId).toString ());
    }
  return keys;
}

void
CodingPlanModel::storeApiKey (const QString &entryId, const QString &apiKey) const
{
  QHash<QString, QString> keys = loadApiKeys ();
  keys.insert (entryId, apiKey);
  saveApiKeys (keys);
}

void
CodingPlanModel::removeApiKey (const QString &entryId) const
{
  QHash<QString, QString> keys = loadApiKeys ();
  if (keys.remove (entryId) == 0)
    {
      return;
    }
  saveApiKeys (keys);
}

void
CodingPlanModel::saveApiKeys (const QHash<QString, QString> &keys) const
{
  QJsonObject object;
  for (auto it = keys.constBegin (); it != keys.constEnd (); ++it)
    {
      object.insert (it.key (), it.value ());
    }

  const QString path = apiKeyFilePath ();
  QDir ().mkpath (QFileInfo (path).absolutePath ());
  QSaveFile file (path);
  if (!file.open (QIODevice::WriteOnly))
    {
      qWarning () << "[coding-plan] cannot open provider key store" << path;
      return;
    }
  file.write (QJsonDocument (object).toJson (QJsonDocument::Indented));
  // QSaveFile ignores setPermissions; enforce owner-only after the commit.
  if (file.commit ())
    {
      QFile::setPermissions (path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
  else
    {
      qWarning () << "[coding-plan] cannot write provider key store" << path;
    }
}

void
CodingPlanModel::onRefreshCompleted (const QString &entryId,
                                     const QuotaSnapshot &snapshot)
{
  if (entryIndex (entryId) < 0)
    {
      return;
    }

  QuotaSnapshot stored = snapshot;
  stored.entryId = entryId;
  stored.label = m_entries.at (entryIndex (entryId)).label;
  m_snapshots.insert (entryId, stored);
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::onRefreshFailed (const QString &entryId, const QString &message,
                                  SnapshotStatus status)
{
  const int index = entryIndex (entryId);
  if (index < 0)
    {
      return;
    }

  const Entry entry = m_entries.at (index);
  const QuotaSnapshot existing = m_snapshots.value (entryId, placeholderSnapshot (entry));

  // A recent good snapshot beats a transient error (orca's stale policy).
  // updatedAt stays at the last success so the keep window really expires;
  // a rejected credential is never transient and shows at once.
  if (status != SnapshotStatus::AuthError && hasGoodData (existing)
      && existing.updatedAt.isValid ()
      && existing.updatedAt.secsTo (QDateTime::currentDateTimeUtc ()) < kStaleKeepSeconds)
    {
      QuotaSnapshot kept = existing;
      kept.message = message;
      m_snapshots.insert (entryId, kept);
      saveSnapshots ();
      emit snapshotsChanged ();
      return;
    }

  QuotaSnapshot snapshot = placeholderSnapshot (entry);
  snapshot.status = status;
  snapshot.message = message.trimmed ().isEmpty ()
      ? QStringLiteral ("读取失败，请稍后重试。")
      : message.trimmed ();
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots.insert (entryId, snapshot);
  saveSnapshots ();
  emit snapshotsChanged ();
}
