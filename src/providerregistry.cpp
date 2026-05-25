// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "providerregistry.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QString
webExtractorScript (const QString &providerId)
{
  const QJsonObject metadata{ { QStringLiteral ("provider"), providerId } };
  const QString metadataJson = QString::fromUtf8 (
      QJsonDocument (metadata).toJson (QJsonDocument::Compact));

  return QStringLiteral (R"JS(
(function() {
  const metadata = %1;
  const text = document.body ? document.body.innerText : "";
  const percentMatch = text.match(/(\d{1,3}(?:\.\d+)?)\s*%%/);
  if (percentMatch) {
    const remainingRatio = Math.max(0, Math.min(100, Number(percentMatch[1]))) / 100;
    return {
      providerId: metadata.provider,
      status: "ok",
      remainingRatio,
      balanceText: percentMatch[0],
      updatedAt: new Date().toISOString()
    };
  }

  return {
    providerId: metadata.provider,
    status: "parse_error",
    message: "未在官方页面中识别到额度百分比，请打开控制台人工确认或手动录入。",
    updatedAt: new Date().toISOString()
  };
})()
)JS")
      .arg (metadataJson);
}

QString
kimiCodeExtractorScript ()
{
  const QJsonObject metadata{ { QStringLiteral ("provider"),
                                QStringLiteral ("kimi-code") } };
  const QString metadataJson = QString::fromUtf8 (
      QJsonDocument (metadata).toJson (QJsonDocument::Compact));

  return QStringLiteral (R"JS(
(function() {
  const metadata = %1;
  var parsed = { weekly: null, fiveHour: null };

  function readQuotaElement(selector) {
    var el = document.querySelector(selector);
    if (!el) return null;
    var text = el.innerText || el.textContent || "";
    var percentMatch = text.match(/(\d{1,3}(?:\.\d+)?)\s*%%/);
    if (percentMatch) {
      return {
        ratio: Math.max(0, Math.min(100, Number(percentMatch[1]))) / 100,
        text: percentMatch[0]
      };
    }
    var numMatch = text.match(/(\d+(?:\.\d+)?)/);
    if (numMatch) {
      return { ratio: null, text: numMatch[0] };
    }
    return null;
  }

  var weekly = readQuotaElement(
    "#app > div > div.app > div > div > main > section.stats-section > div.stats-desktop > div:nth-child(1)"
  );
  var fiveHour = readQuotaElement(
    "#app > div > div.app > div > div > main > section.stats-section > div.stats-desktop > div:nth-child(2)"
  );

  if (!weekly && !fiveHour) {
    return {
      providerId: metadata.provider,
      status: "parse_error",
      message: "未在 Kimi Code 控制台识别到额度信息，请确认已登录且控制台页面已完全加载。",
      updatedAt: new Date().toISOString()
    };
  }

  var result = {
    providerId: metadata.provider,
    status: "ok",
    updatedAt: new Date().toISOString()
  };

  if (weekly) {
    result.remainingRatio = weekly.ratio !== null ? weekly.ratio : -1;
    result.balanceText = weekly.text;
  }
  if (fiveHour) {
    result.fiveHourBalanceText = fiveHour.text;
    if (weekly === null && fiveHour.ratio !== null) {
      result.remainingRatio = fiveHour.ratio;
      result.balanceText = fiveHour.text;
    }
  }

  return result;
})()
)JS")
      .arg (metadataJson);
}
}

PanelSeverity
QuotaSnapshot::severity () const
{
  switch (status)
    {
    case SnapshotStatus::Ok:
    case SnapshotStatus::Warning:
    case SnapshotStatus::Exhausted:
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
        SourceType::WebView,
        QStringLiteral ("https://chatgpt.com/auth/login"),
        QStringLiteral ("https://chatgpt.com/#settings/usage"),
        QStringLiteral ("https://chatgpt.com/#settings/usage"),
        { QStringLiteral ("https://chatgpt.com"),
          QStringLiteral ("https://auth.openai.com"),
          QStringLiteral ("https://platform.openai.com") },
        webExtractorScript (QStringLiteral ("codex")) });

  registry.addProvider (
      { QStringLiteral ("kimi-code"),
        QStringLiteral ("Kimi Code"),
        SourceType::WebView,
        QStringLiteral ("https://www.kimi.com/code/"),
        QStringLiteral ("https://www.kimi.com/code/console"),
        QStringLiteral ("https://www.kimi.com/code/console"),
        { QStringLiteral ("https://www.kimi.com"),
          QStringLiteral ("https://kimi.moonshot.cn"),
          QStringLiteral ("https://platform.moonshot.cn") },
        kimiCodeExtractorScript () });

  registry.addProvider (
      { QStringLiteral ("glm-coding"),
        QStringLiteral ("GLM Coding"),
        SourceType::WebView,
        QStringLiteral ("https://chatglm.cn/login"),
        QStringLiteral ("https://chatglm.cn/"),
        QStringLiteral ("https://open.bigmodel.cn/usercenter/overview"),
        { QStringLiteral ("https://chatglm.cn"),
          QStringLiteral ("https://open.bigmodel.cn") },
        webExtractorScript (QStringLiteral ("glm-coding")) });

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
    case SourceType::WebView:
      return QStringLiteral ("webview");
    case SourceType::OfficialApi:
      return QStringLiteral ("official_api");
    case SourceType::Manual:
      return QStringLiteral ("manual");
    case SourceType::ConsoleLink:
      return QStringLiteral ("console_link");
    }

  return QStringLiteral ("webview");
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
