import { clampRatio, percentText, readTextQuota, buildNormalizeSnapshot } from "../shared/quota-utils.js";

export const glmCodingProvider = {
  id: "glm-coding",
  name: "GLM Coding",
  loginUrl: "https://bigmodel.cn/",
  quotaUrl: "https://bigmodel.cn/coding-plan/personal/usage",
  consoleUrl: "https://bigmodel.cn/coding-plan/personal/usage",
  allowedOrigin: "https://bigmodel.cn",
  loginIndicators: [".user-info", "[data-testid='user-menu']"],
  extractionMode: "tab",

  async extractViaApi(html, doc) {
    return await readQuotaApi();
  },

  extractQuota(doc) {
    let parsed = null;

    try {
      parsed = readQuotaFromDoc(doc);
    } catch {
      parsed = null;
    }

    if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
      parsed = readGlmDomFallback(doc);
    }

    if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
      return {
        success: false,
        reason: "Could not find GLM Coding quota info",
      };
    }

    return {
      success: true,
      raw: parsed,
    };
  },

  normalizeSnapshot(raw) {
    return buildNormalizeSnapshot("glm-coding", "GLM Coding", raw, { includeUsage: true });
  },
};

function cookieValue(doc, name) {
  const prefix = name + "=";
  const parts = doc.cookie.split(";");
  for (const part of parts) {
    const trimmed = part.trim();
    if (trimmed.indexOf(prefix) === 0) {
      return decodeURIComponent(trimmed.slice(prefix.length));
    }
  }
  return "";
}

function limitToQuota(limit) {
  if (!limit) return null;
  const usedRatio = clampRatio(Number(limit.percentage) / 100);
  const remainingRatio = usedRatio >= 0 ? 1 - usedRatio : -1;
  return {
    ratio: remainingRatio,
    text: percentText(remainingRatio),
    used: Number(limit.percentage),
    total: 100,
    resetAt: Number.isFinite(Number(limit.nextResetTime))
      ? new Date(Number(limit.nextResetTime)).toISOString()
      : "",
  };
}

async function readQuotaApi() {
  const response = await fetch("https://bigmodel.cn/api/monitor/usage/quota/limit", {
    credentials: "include",
  });

  if (!response.ok) {
    return null;
  }

  const payload = await response.json().catch(() => null);
  if (!payload) return null;
  const limits = payload?.data?.limits || [];
  const fiveHour = limits.find((item) => item && item.type === "TOKENS_LIMIT" && item.unit === 3);
  const weekly = limits.find((item) => item && item.type === "TOKENS_LIMIT" && item.unit === 6);

  const weeklyQuota = limitToQuota(weekly);
  const fiveHourQuota = limitToQuota(fiveHour);

  if (!weeklyQuota && !fiveHourQuota) return null;

  return {
    weekly: weeklyQuota,
    fiveHour: fiveHourQuota,
  };
}

function readQuotaFromDoc(doc) {
  const token = cookieValue(doc, "bigmodel_token_production");
  if (!token) return null;

  const request = new XMLHttpRequest();
  request.open("GET", "/api/monitor/usage/quota/limit", false);
  request.setRequestHeader("Authorization", token);
  request.send();

  if (request.status < 200 || request.status >= 300) return null;

  const payload = JSON.parse(request.responseText);
  const limits = payload?.data?.limits || [];
  const fiveHour = limits.find((item) => item && item.type === "TOKENS_LIMIT" && item.unit === 3);
  const weekly = limits.find((item) => item && item.type === "TOKENS_LIMIT" && item.unit === 6);

  return {
    weekly: limitToQuota(weekly),
    fiveHour: limitToQuota(fiveHour),
  };
}

function readGlmDomFallback(doc) {
  return {
    weekly: readTextQuota(doc, "每周使用额度"),
    fiveHour: readTextQuota(doc, "每5小时使用额度"),
  };
}
