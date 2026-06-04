// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providerregistry.h"

PanelSeverity
QuotaSnapshot::severity () const
{
  switch (status)
    {
    case SnapshotStatus::Ok:
    case SnapshotStatus::Warning:
    case SnapshotStatus::Exhausted:
      break;
    case SnapshotStatus::Authenticated:
      if (remainingRatio < 0)
        {
          return PanelSeverity::Warning;
        }
      break;
    case SnapshotStatus::AuthError:
    case SnapshotStatus::RateLimited:
    case SnapshotStatus::Unsupported:
    case SnapshotStatus::ParseError:
    case SnapshotStatus::NetworkError:
      return PanelSeverity::Error;
    }

  if (remainingRatio < 0)
    {
      return PanelSeverity::Error;
    }

  if (remainingRatio < 0.10)
    {
      return PanelSeverity::Critical;
    }

  if (remainingRatio <= 0.30)
    {
      return PanelSeverity::Warning;
    }

  return PanelSeverity::Normal;
}

QVariantMap
QuotaSnapshot::toVariantMap () const
{
  QVariantMap result;
  result.insert (QStringLiteral ("providerId"), providerId);
  result.insert (QStringLiteral ("providerName"), providerName);
  result.insert (QStringLiteral ("source"), sourceTypeToString (source));
  result.insert (QStringLiteral ("status"), snapshotStatusToString (status));
  result.insert (QStringLiteral ("severity"), panelSeverityToString (severity ()));
  result.insert (QStringLiteral ("remainingRatio"), remainingRatio);
  result.insert (QStringLiteral ("used"), used);
  result.insert (QStringLiteral ("total"), total);
  result.insert (QStringLiteral ("unit"), unit);
  result.insert (QStringLiteral ("balanceText"), balanceText);
  result.insert (QStringLiteral ("fiveHourRemainingRatio"), fiveHourRemainingRatio);
  result.insert (QStringLiteral ("fiveHourBalanceText"), fiveHourBalanceText);
  result.insert (QStringLiteral ("resetAt"), resetAt.toString (Qt::ISODate));
  result.insert (QStringLiteral ("updatedAt"), updatedAt.toString (Qt::ISODate));
  result.insert (QStringLiteral ("consoleUrl"), consoleUrl);
  result.insert (QStringLiteral ("message"), message);
  return result;
}

ProviderRegistry
ProviderRegistry::createDefault ()
{
  ProviderRegistry registry;

  registry.addProvider (
      { QStringLiteral ("codex"),
        QStringLiteral ("Codex / ChatGPT"),
        SourceType::BrowserExt,
        QStringLiteral ("https://chatgpt.com/auth/login"),
        QStringLiteral (
            "https://chatgpt.com/codex/cloud/settings/analytics#usage") });

  registry.addProvider (
      { QStringLiteral ("kimi-code"),
        QStringLiteral ("Kimi Code"),
        SourceType::BrowserExt,
        QStringLiteral ("https://www.kimi.com/code/"),
        QStringLiteral ("https://www.kimi.com/code/console") });

  registry.addProvider (
      { QStringLiteral ("glm-coding"),
        QStringLiteral ("GLM Coding"),
        SourceType::BrowserExt,
        QStringLiteral ("https://bigmodel.cn/"),
        QStringLiteral ("https://bigmodel.cn/coding-plan/personal/usage") });

  registry.addProvider (
      { QStringLiteral ("minimax"),
        QStringLiteral ("MiniMax Coding"),
        SourceType::BrowserExt,
        QStringLiteral ("https://platform.minimaxi.com/"),
        QStringLiteral (
            "https://platform.minimaxi.com/user-center/billing") });

  return registry;
}

QStringList
ProviderRegistry::providerIds () const
{
  return m_orderedIds;
}

bool
ProviderRegistry::contains (const QString &providerId) const
{
  return m_providers.contains (providerId);
}

ProviderDefinition
ProviderRegistry::provider (const QString &providerId) const
{
  return m_providers.value (providerId);
}

QList<ProviderDefinition>
ProviderRegistry::providers () const
{
  QList<ProviderDefinition> result;
  result.reserve (m_orderedIds.size ());
  for (const QString &id : m_orderedIds)
    {
      result.append (m_providers.value (id));
    }
  return result;
}

void
ProviderRegistry::addProvider (const ProviderDefinition &provider)
{
  if (provider.id.trimmed ().isEmpty () || m_providers.contains (provider.id))
    {
      return;
    }

  m_orderedIds.append (provider.id);
  m_providers.insert (provider.id, provider);
}

QString
sourceTypeToString (SourceType sourceType)
{
  switch (sourceType)
    {
    case SourceType::BrowserExt:
      return QStringLiteral ("browser_ext");
    case SourceType::OfficialApi:
      return QStringLiteral ("official_api");
    case SourceType::Manual:
      return QStringLiteral ("manual");
    case SourceType::ConsoleLink:
      return QStringLiteral ("console_link");
    }

  return QStringLiteral ("browser_ext");
}

QString
snapshotStatusToString (SnapshotStatus status)
{
  switch (status)
    {
    case SnapshotStatus::Ok:
      return QStringLiteral ("ok");
    case SnapshotStatus::Warning:
      return QStringLiteral ("warning");
    case SnapshotStatus::Exhausted:
      return QStringLiteral ("exhausted");
    case SnapshotStatus::AuthError:
      return QStringLiteral ("auth_error");
    case SnapshotStatus::Authenticated:
      return QStringLiteral ("authenticated");
    case SnapshotStatus::RateLimited:
      return QStringLiteral ("rate_limited");
    case SnapshotStatus::Unsupported:
      return QStringLiteral ("unsupported");
    case SnapshotStatus::ParseError:
      return QStringLiteral ("parse_error");
    case SnapshotStatus::NetworkError:
      return QStringLiteral ("network_error");
    }

  return QStringLiteral ("unsupported");
}

QString
panelSeverityToString (PanelSeverity severity)
{
  switch (severity)
    {
    case PanelSeverity::Normal:
      return QStringLiteral ("normal");
    case PanelSeverity::Warning:
      return QStringLiteral ("warning");
    case PanelSeverity::Critical:
      return QStringLiteral ("critical");
    case PanelSeverity::Error:
      return QStringLiteral ("error");
    }

  return QStringLiteral ("error");
}
