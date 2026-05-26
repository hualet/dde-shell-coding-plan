// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "codingplanmodel.h"

#include <QDateTime>
#include <QDesktopServices>
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
constexpr int kRefreshIntervalMs = 30 * 60 * 1000;

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
      item.insert (QStringLiteral ("quotaUrl"), provider.quotaUrl);
      item.insert (QStringLiteral ("consoleUrl"), provider.consoleUrl);
      item.insert (QStringLiteral ("allowedOrigins"), provider.allowedOrigins);
      item.insert (QStringLiteral ("extractorScript"), provider.extractorScript);
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
      result.append (snapshot.toVariantMap ());
    }
  return result;
}

bool
CodingPlanModel::hasSubscriptions () const
{
  return !m_snapshots.isEmpty ();
}

QString
CodingPlanModel::tooltipText () const
{
  QStringList lines;
  for (const QuotaSnapshot &snapshot : m_snapshots)
    {
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
  for (const ProviderDefinition &provider : m_registry.providers ())
    {
      refreshProvider (provider.id);
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

  const ProviderDefinition provider = m_registry.provider (providerId);
  QuotaSnapshot snapshot = m_snapshots.at (index);
  snapshot.status = SnapshotStatus::AuthError;
  snapshot.message = QStringLiteral ("请先在内置 WebView 中登录官方页面，然后刷新读取额度。");
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
CodingPlanModel::setManualRatio (const QString &providerId, double ratio)
{
  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
      return;
    }

  QuotaSnapshot snapshot = m_snapshots.at (index);
  snapshot.remainingRatio = std::max (0.0, std::min (1.0, ratio));
  snapshot.status = SnapshotStatus::Ok;
  snapshot.balanceText = QStringLiteral ("%1%").arg (qRound (snapshot.remainingRatio * 100));
  snapshot.message = QStringLiteral ("手动录入");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::setWebViewResult (const QString &providerId,
                                   const QVariantMap &result)
{
  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
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

  snapshot.status = SnapshotStatus::Ok;
  snapshot.message = QStringLiteral ("WebView 读取");
  snapshot.updatedAt = QDateTime::currentDateTimeUtc ();
  m_snapshots[index] = snapshot;
  saveSnapshots ();
  emit snapshotsChanged ();
}

void
CodingPlanModel::setProviderError (const QString &providerId,
                                   const QString &message)
{
  const int index = snapshotIndex (providerId);
  if (index < 0)
    {
      return;
    }

  QuotaSnapshot snapshot = m_snapshots.at (index);
  snapshot.status = SnapshotStatus::ParseError;
  snapshot.message = message.trimmed ().isEmpty ()
                         ? QStringLiteral ("读取失败，请打开控制台人工确认或手动录入。")
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
  QSettings settings (QString::fromLatin1 (kSettingsOrganization),
                      QString::fromLatin1 (kSettingsApplication));
  const QJsonDocument document = QJsonDocument::fromJson (
      settings.value (QString::fromLatin1 (kSnapshotsKey)).toByteArray ());

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
    }
}

void
CodingPlanModel::saveSnapshots () const
{
  QJsonArray array;
  for (const QuotaSnapshot &snapshot : m_snapshots)
    {
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
