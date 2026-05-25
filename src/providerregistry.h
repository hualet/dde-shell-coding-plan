// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

enum class SourceType
{
  WebView,
  OfficialApi,
  Manual,
  ConsoleLink,
};

enum class SnapshotStatus
{
  Ok,
  Warning,
  Exhausted,
  AuthError,
  RateLimited,
  Unsupported,
  ParseError,
  NetworkError,
};

enum class PanelSeverity
{
  Normal,
  Warning,
  Critical,
  Error,
};

struct ProviderDefinition
{
  QString id;
  QString name;
  SourceType sourceType = SourceType::WebView;
  QString loginUrl;
  QString quotaUrl;
  QString consoleUrl;
  QStringList allowedOrigins;
  QString extractorScript;
};

struct QuotaSnapshot
{
  QString providerId;
  QString providerName;
  SourceType source = SourceType::WebView;
  SnapshotStatus status = SnapshotStatus::Unsupported;
  double remainingRatio = -1.0;
  double fiveHourRemainingRatio = -1.0;
  double used = -1.0;
  double total = -1.0;
  QString unit;
  QString balanceText;
  QString fiveHourBalanceText;
  QDateTime resetAt;
  QDateTime updatedAt;
  QString consoleUrl;
  QString message;

  PanelSeverity severity () const;
  QVariantMap toVariantMap () const;
};

class ProviderRegistry
{
public:
  static ProviderRegistry createDefault ();

  QStringList providerIds () const;
  bool contains (const QString &providerId) const;
  ProviderDefinition provider (const QString &providerId) const;
  QList<ProviderDefinition> providers () const;

private:
  void addProvider (const ProviderDefinition &provider);

  QHash<QString, ProviderDefinition> m_providers;
  QStringList m_orderedIds;
};

QString sourceTypeToString (SourceType sourceType);
QString snapshotStatusToString (SnapshotStatus status);
QString panelSeverityToString (PanelSeverity severity);
