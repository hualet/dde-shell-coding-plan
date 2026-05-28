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

  function clampRatio(value) {
    if (!Number.isFinite(value)) return -1;
    return Math.max(0, Math.min(1, value));
  }

  function percentText(ratio) {
    if (!Number.isFinite(ratio) || ratio < 0) return "";
    return Math.round(ratio * 100) + "%%";
  }

  function detailToQuota(detail) {
    if (!detail) return null;
    var used = Number(detail.used);
    var limit = Number(detail.limit);
    if (!Number.isFinite(used) || !Number.isFinite(limit) || limit <= 0) {
      return null;
    }
    var remaining = Number(detail.remaining);
    if (!Number.isFinite(remaining)) {
      remaining = limit - used;
    }
    var remainingRatio = clampRatio(remaining / limit);
    return {
      ratio: remainingRatio,
      text: percentText(remainingRatio),
      used: used,
      total: limit,
      resetAt: detail.resetTime || ""
    };
  }

  function readBillingUsage() {
    var token = localStorage.getItem('access_token');
    if (!token) return null;

    var request = new XMLHttpRequest();
    request.open(
      "POST",
      "/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages",
      false
    );
    request.setRequestHeader("content-type", "application/json");
    request.setRequestHeader("authorization", "Bearer " + token);
    request.setRequestHeader("x-msh-platform", "web");
    request.setRequestHeader("x-msh-version", "1.0.0");
    request.setRequestHeader("x-language", "zh-CN");
    request.send(JSON.stringify({ scope: ["FEATURE_CODING"] }));

    if (request.status < 200 || request.status >= 300) return null;

    var payload = JSON.parse(request.responseText);
    var usages = Array.isArray(payload.usages) ? payload.usages : [];
    var usage = usages.find(function(item) {
      return item && item.scope === "FEATURE_CODING";
    }) || usages[0];

    if (!usage) return null;

    return {
      weekly: detailToQuota(usage.detail),
      fiveHour: detailToQuota(
        Array.isArray(usage.limits) && usage.limits.length > 0
          ? usage.limits[0].detail
          : null
      )
    };
  }

  function readTextQuota(label) {
    var text = document.body ? document.body.innerText : "";
    var labelIndex = text.indexOf(label);
    if (labelIndex < 0) return null;

    var section = text.slice(labelIndex, labelIndex + 200);
    var percentMatch = section.match(/(\d{1,3}(?:\.\d+)?)\s*%%/);
    if (!percentMatch) return null;

    var usedRatio = clampRatio(Number(percentMatch[1]) / 100);
    var remainingRatio = usedRatio >= 0 ? 1 - usedRatio : -1;
    return {
      ratio: remainingRatio,
      text: percentText(remainingRatio),
      used: -1,
      total: -1,
      resetAt: ""
    };
  }

  function readDomFallback() {
    return {
      weekly: readTextQuota("本周用量"),
      fiveHour: readTextQuota("频限明细")
    };
  }

  var parsed = null;
  try {
    parsed = readBillingUsage();
  } catch (error) {
    parsed = null;
  }

  if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
    parsed = readDomFallback();
  }

  if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
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

  if (parsed.weekly) {
    result.remainingRatio = parsed.weekly.ratio;
    result.balanceText = parsed.weekly.text;
    if (parsed.weekly.used >= 0) {
      result.used = parsed.weekly.used;
    }
    if (parsed.weekly.total >= 0) {
      result.total = parsed.weekly.total;
    }
    if (parsed.weekly.resetAt) {
      result.resetAt = parsed.weekly.resetAt;
    }
  }

  if (parsed.fiveHour) {
    result.fiveHourBalanceText = parsed.fiveHour.text;
    result.fiveHourRemainingRatio = parsed.fiveHour.ratio;
    if (!parsed.weekly) {
      result.remainingRatio = parsed.fiveHour.ratio;
      result.balanceText = parsed.fiveHour.text;
    }
  }

  return result;
})()
)JS")
      .arg (metadataJson);
}

QString
glmCodingExtractorScript ()
{
  const QJsonObject metadata{ { QStringLiteral ("provider"),
                                QStringLiteral ("glm-coding") } };
  const QString metadataJson = QString::fromUtf8 (
      QJsonDocument (metadata).toJson (QJsonDocument::Compact));

  return QStringLiteral (R"JS(
(function() {
  const metadata = %1;

  function clampRatio(value) {
    if (!Number.isFinite(value)) return -1;
    return Math.max(0, Math.min(1, value));
  }

  function percentText(ratio) {
    if (!Number.isFinite(ratio) || ratio < 0) return "";
    return Math.round(ratio * 100) + "%%";
  }

  function cookieValue(name) {
    var prefix = name + "=";
    var parts = document.cookie.split(";");
    for (var index = 0; index < parts.length; ++index) {
      var part = parts[index].trim();
      if (part.indexOf(prefix) === 0) {
        return decodeURIComponent(part.slice(prefix.length));
      }
    }
    return "";
  }

  function limitToQuota(limit) {
    if (!limit) return null;
    var usedRatio = clampRatio(Number(limit.percentage) / 100);
    var remainingRatio = usedRatio >= 0 ? 1 - usedRatio : -1;
    return {
      ratio: remainingRatio,
      text: percentText(remainingRatio),
      used: Number(limit.percentage),
      total: 100,
      resetAt: Number.isFinite(Number(limit.nextResetTime))
        ? new Date(Number(limit.nextResetTime)).toISOString()
        : ""
    };
  }

  function readQuotaLimit() {
    var token = cookieValue("bigmodel_token_production");
    if (!token) return null;

    var request = new XMLHttpRequest();
    request.open("GET", "/api/monitor/usage/quota/limit", false);
    request.setRequestHeader("Authorization", token);
    request.send();

    if (request.status < 200 || request.status >= 300) return null;

    var payload = JSON.parse(request.responseText);
    var limits = payload && payload.data && Array.isArray(payload.data.limits)
      ? payload.data.limits
      : [];
    var fiveHour = limits.find(function(item) {
      return item && item.type === "TOKENS_LIMIT" && item.unit === 3;
    });
    var weekly = limits.find(function(item) {
      return item && item.type === "TOKENS_LIMIT" && item.unit === 6;
    });

    return {
      weekly: limitToQuota(weekly),
      fiveHour: limitToQuota(fiveHour)
    };
  }

  function readTextQuota(label) {
    var text = document.body ? document.body.innerText : "";
    var labelIndex = text.indexOf(label);
    if (labelIndex < 0) return null;

    var section = text.slice(labelIndex, labelIndex + 200);
    var percentMatch = section.match(/(\d{1,3}(?:\.\d+)?)\s*%%/);
    if (!percentMatch) return null;

    var usedRatio = clampRatio(Number(percentMatch[1]) / 100);
    var remainingRatio = usedRatio >= 0 ? 1 - usedRatio : -1;
    return {
      ratio: remainingRatio,
      text: percentText(remainingRatio),
      used: -1,
      total: -1,
      resetAt: ""
    };
  }

  function readDomFallback() {
    return {
      weekly: readTextQuota("每周使用额度"),
      fiveHour: readTextQuota("每5小时使用额度")
    };
  }

  var parsed = null;
  try {
    parsed = readQuotaLimit();
  } catch (error) {
    parsed = null;
  }

  if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
    parsed = readDomFallback();
  }

  if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
    return {
      providerId: metadata.provider,
      status: "parse_error",
      message: "未在 GLM Coding 用量页识别到额度信息，请确认已登录且用量页面已完全加载。",
      updatedAt: new Date().toISOString()
    };
  }

  var result = {
    providerId: metadata.provider,
    status: "ok",
    updatedAt: new Date().toISOString()
  };

  if (parsed.weekly) {
    result.remainingRatio = parsed.weekly.ratio;
    result.balanceText = parsed.weekly.text;
    if (parsed.weekly.used >= 0) result.used = parsed.weekly.used;
    if (parsed.weekly.total >= 0) result.total = parsed.weekly.total;
    if (parsed.weekly.resetAt) result.resetAt = parsed.weekly.resetAt;
  }

  if (parsed.fiveHour) {
    result.fiveHourRemainingRatio = parsed.fiveHour.ratio;
    result.fiveHourBalanceText = parsed.fiveHour.text;
    if (!parsed.weekly) {
      result.remainingRatio = parsed.fiveHour.ratio;
      result.balanceText = parsed.fiveHour.text;
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
        QStringLiteral ("https://bigmodel.cn/"),
        QStringLiteral ("https://bigmodel.cn/coding-plan/personal/usage"),
        QStringLiteral ("https://bigmodel.cn/coding-plan/personal/usage"),
        { QStringLiteral ("https://bigmodel.cn"),
          QStringLiteral ("https://open.bigmodel.cn") },
        glmCodingExtractorScript () });

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
