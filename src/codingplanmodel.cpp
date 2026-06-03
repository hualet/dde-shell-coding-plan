// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "codingplanmodel.h"
#include "websocket_server.h"
#include "browser_ext_provider.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <algorithm>

namespace
{
constexpr auto kSettingsOrganization = "deepin";
constexpr auto kSettingsApplication = "dde-shell-coding-plan";
constexpr auto kSnapshotsKey = "snapshots";
constexpr int kRefreshIntervalMs = 5 * 60 * 1000;

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
}

CodingPlanModel::CodingPlanModel (QObject *parent)
    : QObject (parent), m_registry (ProviderRegistry::createDefault ())
{
  loadSnapshots ();

  m_refreshTimer.setInterval (kRefreshIntervalMs);
  connect (&m_refreshTimer, &QTimer::timeout, this,
           &CodingPlanModel::refreshAll);
  m_refreshTimer.start ();

  m_debounceTimer.setInterval (500);
  m_debounceTimer.setSingleShot (true);
  connect (&m_debounceTimer, &QTimer::timeout, this, [this]() {
    loadSnapshots ();
    emit snapshotsChanged ();
  });
}

QVariantList
CodingPlanModel::providers () const
{
  QVariantList result;
  for (const ProviderDefinition &provider : m_registry.providers ())
    {
      if (!isProviderEnabled (provider.id))
        {
          continue;
        }
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
CodingPlanModel::snapshots () const
{
  QVariantList result;
  for (const QuotaSnapshot &snapshot : m_snapshots)
    {
      if (!isProviderEnabled (snapshot.providerId))
        {
          continue;
        }
      result.append (snapshot.toVariantMap ());
    }
  return result;
}

bool
CodingPlanModel::hasSubscriptions () const
{
  for (const QuotaSnapshot &snapshot : m_snapshots)
    {
      if (isProviderEnabled (snapshot.providerId))
        {
          return true;
        }
    }
  return false;
}

QString
CodingPlanModel::tooltipText () const
{
  QStringList lines;
  for (const QuotaSnapshot &snapshot : m_snapshots)
    {
      if (!isProviderEnabled (snapshot.providerId))
        {
          continue;
        }
      const QString value
          = snapshot.remainingRatio >= 0
                ? QStringLiteral ("%1%").arg (qRound (snapshot.remainingRatio * 100))
                : snapshot.message;
      lines.append (QStringLiteral ("%1: %2").arg (snapshot.providerName, value));
    }

  return lines.join (QLatin1Char ('\n'));
}

void
CodingPlanModel::refreshAll ()
{
  if (m_browserExtProvider && m_browserExtProvider->isExtensionConnected ())
    {
      m_browserExtProvider->refreshProviders (m_registry.providerIds ());
    }
  else
    {
      emit backgroundRefreshRequested ();
    }
}

void
CodingPlanModel::refreshProvider (const QString &providerId)
{
  const int index = snapshotIndex (providerId);
  if (index < 0 || !m_registry.contains (providerId))
    {
      return;
    }

  if (m_browserExtProvider && m_browserExtProvider->isExtensionConnected ())
    {
      m_browserExtProvider->refreshProviders ({providerId});
      return;
    }

  const ProviderDefinition provider = m_registry.provider (providerId);
  QuotaSnapshot snapshot = m_snapshots.at (index);

  if (snapshot.status == SnapshotStatus::Ok
      || snapshot.status == SnapshotStatus::Warning
      || snapshot.status == SnapshotStatus::Exhausted)
    {
      snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
      m_snapshots[index] = snapshot;
      saveSnapshots ();
      emit snapshotsChanged ();
      return;
    }

  snapshot.status = SnapshotStatus::AuthError;
  snapshot.message = QStringLiteral ("请安装浏览器扩展并完成配对，然后刷新读取额度。");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  snapshot.consoleUrl = provider.consoleUrl;
  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
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
CodingPlanModel::openLogin (const QString &providerId)
{
  if (!m_registry.contains (providerId))
    {
      return;
    }

  QDesktopServices::openUrl (QUrl (m_registry.provider (providerId).loginUrl));
}

void
CodingPlanModel::clearSession (const QString &providerId)
{
  emit sessionCleared (providerId);
}

void
CodingPlanModel::setBrowserExtResult (const QString &providerId,
                                      const QVariantMap &result)
{
  qWarning () << "[coding-plan][model] setBrowserExtResult input" << providerId
            << "status" << result.value (QStringLiteral ("status")).toString ()
            << "remaining" << result.value (QStringLiteral ("remainingRatio"))
            << "fiveHour" << result.value (QStringLiteral ("fiveHourRemainingRatio"))
            << "balanceText" << result.value (QStringLiteral ("balanceText")).toString ()
            << "fiveHourText" << result.value (QStringLiteral ("fiveHourBalanceText")).toString ()
            << "message" << result.value (QStringLiteral ("message")).toString ();

  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
      qWarning () << "[coding-plan][model] setBrowserExtResult ignored unknown provider"
                 << providerId;
      return;
    }

  QuotaSnapshot snapshot = m_snapshots.at (index);

  if (result.contains (QStringLiteral ("remainingRatio")))
    {
      bool ok = false;
      const double weeklyRatio = result.value (QStringLiteral ("remainingRatio")).toDouble (&ok);
      snapshot.remainingRatio = (ok && weeklyRatio >= 0) ? std::max (0.0, std::min (1.0, weeklyRatio)) : -1.0;
    }
  else
    {
      snapshot.remainingRatio = -1.0;
    }
  snapshot.balanceText = result.value (QStringLiteral ("balanceText")).toString ();

  if (result.contains (QStringLiteral ("fiveHourRemainingRatio")))
    {
      bool ok = false;
      const double fiveHourRatio = result.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (&ok);
      snapshot.fiveHourRemainingRatio = (ok && fiveHourRatio >= 0) ? std::max (0.0, std::min (1.0, fiveHourRatio)) : -1.0;
    }
  else
    {
      snapshot.fiveHourRemainingRatio = -1.0;
    }
  snapshot.fiveHourBalanceText = result.value (QStringLiteral ("fiveHourBalanceText")).toString ();

  qWarning () << "[coding-plan][model] setBrowserExtResult parsed" << providerId
            << "remaining" << snapshot.remainingRatio
            << "fiveHour" << snapshot.fiveHourRemainingRatio
            << "balanceText" << snapshot.balanceText
            << "fiveHourText" << snapshot.fiveHourBalanceText;

  snapshot.status = SnapshotStatus::Ok;
  snapshot.message = QStringLiteral ("浏览器扩展读取");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::setProviderAuthenticated (const QString &providerId)
{
  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
      return;
    }

  QuotaSnapshot snapshot = m_snapshots.at (index);
  if (snapshot.status == SnapshotStatus::Ok
      || snapshot.status == SnapshotStatus::Warning
      || snapshot.status == SnapshotStatus::Exhausted)
    {
      return;
    }

  snapshot.status = SnapshotStatus::Authenticated;
  snapshot.message = QStringLiteral ("已登录，等待读取额度");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::setProviderError (const QString &providerId,
                                    const QString &message)
{
  qWarning () << "[coding-plan][model] setProviderError" << providerId
            << "message" << message;

  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
      qWarning () << "[coding-plan][model] setProviderError ignored unknown provider"
                << providerId;
      return;
    }

  QuotaSnapshot snapshot = m_snapshots.at (index);
  if (snapshot.status != SnapshotStatus::Authenticated)
    {
      snapshot.status = SnapshotStatus::ParseError;
    }
  snapshot.remainingRatio = -1.0;
  snapshot.balanceText.clear ();
  snapshot.fiveHourRemainingRatio = -1.0;
  snapshot.fiveHourBalanceText.clear ();
  snapshot.message = message.trimmed ().isEmpty ()
                         ? QStringLiteral ("读取失败，请打开控制台人工确认。")
                         : message.trimmed ();
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
}

QuotaSnapshot
CodingPlanModel::createInitialSnapshot (const ProviderDefinition &provider) const
{
  QuotaSnapshot snapshot;
  snapshot.providerId = provider.id;
  snapshot.providerName = provider.name;
  snapshot.source = provider.sourceType;
  snapshot.status = SnapshotStatus::AuthError;
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  snapshot.consoleUrl = provider.consoleUrl;
  snapshot.message = QStringLiteral ("等待登录");
  return snapshot;
}

void
CodingPlanModel::loadSnapshots ()
{
  m_snapshots.clear ();

  QSettings settings (QString::fromLatin1 (kSettingsOrganization),
                      QString::fromLatin1 (kSettingsApplication));
  const QByteArray raw = settings.value (QString::fromLatin1 (kSnapshotsKey)).toByteArray ();
  qDebug () << "CodingPlanModel::loadSnapshots raw size:" << raw.size ();

  const QJsonDocument document = QJsonDocument::fromJson (raw);
  const QJsonArray storedSnapshots = document.array ();
  for (const ProviderDefinition &provider : m_registry.providers ())
    {
      QuotaSnapshot snapshot = createInitialSnapshot (provider);
      for (const QJsonValue &value : storedSnapshots)
        {
          const QJsonObject object = value.toObject ();
          if (object.value (QStringLiteral ("providerId")).toString () != provider.id)
            {
              continue;
            }

          const QString sourceStr = object.value (QStringLiteral ("source")).toString ();
          if (sourceStr == QStringLiteral ("webview"))
            {
              snapshot.source = SourceType::BrowserExt;
            }
          else
            {
              snapshot.source = (sourceStr == QStringLiteral ("official_api"))
                  ? SourceType::OfficialApi
                  : (sourceStr == QStringLiteral ("manual"))
                      ? SourceType::Manual
                      : SourceType::BrowserExt;
            }

          snapshot.status = statusFromString (object.value (QStringLiteral ("status")).toString ());
          snapshot.remainingRatio = object.value (QStringLiteral ("remainingRatio")).toDouble (-1.0);
          snapshot.balanceText = object.value (QStringLiteral ("balanceText")).toString ();
          snapshot.fiveHourRemainingRatio = object.value (QStringLiteral ("fiveHourRemainingRatio")).toDouble (-1.0);
          snapshot.fiveHourBalanceText = object.value (QStringLiteral ("fiveHourBalanceText")).toString ();
          snapshot.message = object.value (QStringLiteral ("message")).toString ();
          snapshot.updatedAt = QDateTime::fromString (
              object.value (QStringLiteral ("updatedAt")).toString (), Qt::ISODate);
          break;
        }
      m_snapshots.append (snapshot);
      qDebug () << "CodingPlanModel::loadSnapshots loaded" << provider.id
                << "status:" << snapshotStatusToString (snapshot.status)
                << "ratio:" << snapshot.remainingRatio;
    }
}

void
CodingPlanModel::saveSnapshots () const
{
  QJsonArray array;
  for (const QuotaSnapshot &snapshot : m_snapshots)
    {
      qDebug () << "CodingPlanModel::saveSnapshots saving" << snapshot.providerId
                << "status:" << snapshotStatusToString (snapshot.status)
                << "ratio:" << snapshot.remainingRatio;
      QJsonObject object;
      object.insert (QStringLiteral ("providerId"), snapshot.providerId);
      object.insert (QStringLiteral ("status"), snapshotStatusToString (snapshot.status));
      object.insert (QStringLiteral ("remainingRatio"), snapshot.remainingRatio);
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

int
CodingPlanModel::snapshotIndex (const QString &providerId) const
{
  for (int index = 0; index < m_snapshots.size (); ++index)
    {
      if (m_snapshots.at (index).providerId == providerId)
        {
          return index;
        }
    }

  return -1;
}

bool
CodingPlanModel::extensionConnected () const
{
  return m_browserExtProvider && m_browserExtProvider->isExtensionConnected ();
}

QString
CodingPlanModel::extensionToken () const
{
  return m_wsServer ? m_wsServer->token () : QString ();
}

void
CodingPlanModel::setWebSocketServer (WebSocketServer *server)
{
  m_wsServer = server;

  if (m_wsServer)
    {
      m_browserExtProvider = new BrowserExtProvider (m_wsServer, this);

      connect (m_browserExtProvider, &BrowserExtProvider::refreshCompleted,
               this, &CodingPlanModel::onRefreshCompleted);
      connect (m_browserExtProvider, &BrowserExtProvider::refreshFailed,
               this, &CodingPlanModel::onRefreshFailed);
      connect (m_browserExtProvider, &BrowserExtProvider::extensionStatusChanged,
               this, &CodingPlanModel::onExtensionStatusChanged);
      connect (m_browserExtProvider, &BrowserExtProvider::availableProvidersChanged,
               this, &CodingPlanModel::onAvailableProvidersChanged);
    }
}

void
CodingPlanModel::onRefreshCompleted (const QString &providerId,
                                     const QuotaSnapshot &snapshot)
{
  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
      return;
    }

  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::onRefreshFailed (const QString &providerId,
                                  const QString &message)
{
  setProviderError (providerId, message);
}

void
CodingPlanModel::onExtensionStatusChanged (bool connected)
{
  Q_UNUSED (connected)
  if (!connected)
    {
      m_enabledProviders.clear ();
      m_hasProviderFilter = false;
      emit providersChanged ();
      emit snapshotsChanged ();
    }
  emit extensionStatusChanged ();
}

void
CodingPlanModel::onAvailableProvidersChanged (const QStringList &providers)
{
  m_enabledProviders = providers;
  m_hasProviderFilter = true;
  emit providersChanged ();
  emit snapshotsChanged ();
}

bool
CodingPlanModel::isProviderEnabled (const QString &providerId) const
{
  if (!m_hasProviderFilter)
    {
      return true;
    }
  return m_enabledProviders.contains (providerId);
}

void
CodingPlanModel::watchExternalChanges ()
{
  if (m_settingsWatcher)
    {
      return;
    }

  const QSettings settings (QString::fromLatin1 (kSettingsOrganization),
                            QString::fromLatin1 (kSettingsApplication));
  const QString filePath = settings.fileName ();

  m_settingsWatcher = new QFileSystemWatcher (this);
  m_settingsWatcher->addPath (filePath);

  const QFileInfo fi (filePath);
  const QString dirPath = fi.absolutePath ();
  m_settingsWatcher->addPath (dirPath);

  connect (m_settingsWatcher, &QFileSystemWatcher::fileChanged, this,
           [this, filePath]() {
             if (!m_settingsWatcher->files ().contains (filePath))
               {
                 m_settingsWatcher->addPath (filePath);
               }
             m_debounceTimer.start ();
           });

  connect (m_settingsWatcher, &QFileSystemWatcher::directoryChanged, this,
           [this, filePath]() {
             if (!m_settingsWatcher->files ().contains (filePath))
               {
                 m_settingsWatcher->addPath (filePath);
               }
             m_debounceTimer.start ();
           });
}
