import { clampRatio, percentText, readTextQuota, buildNormalizeSnapshot } from "../shared/quota-utils.js";

export { detailToQuota };

export const kimiCodeProvider = {
  id: "kimi-code",
  name: "Kimi Code",
  loginUrl: "https://www.kimi.com/code/",
  quotaUrl: "https://www.kimi.com/code/console",
  consoleUrl: "https://www.kimi.com/code/console",
  allowedOrigin: "https://www.kimi.com",
  loginIndicators: [".user-avatar", "[data-testid='avatar']"],
  extractionMode: "tab",

  async extractViaApi(html, doc) {
    return await readBillingApi();
  },

  extractQuota(doc) {
    let parsed = null;

    try {
      parsed = readBillingFromDoc(doc);
    } catch {
      parsed = null;
    }

    if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
      parsed = readKimiDomFallback(doc);
    } else if (parsed && parsed.weekly && !parsed.fiveHour) {
      const fallback = readKimiDomFallback(doc);
      if (fallback && fallback.fiveHour) {
        parsed.fiveHour = fallback.fiveHour;
      }
    }

    if (!parsed || (!parsed.weekly && !parsed.fiveHour)) {
      return {
        success: false,
        reason: "Could not find Kimi Code quota info",
      };
    }

    return {
      success: true,
      raw: parsed,
    };
  },

  normalizeSnapshot(raw) {
    return buildNormalizeSnapshot("kimi-code", "Kimi Code", raw, { includeUsage: true });
  },
};

function detailToQuota(detail) {
  if (!detail) return null;
  if (detail.used === "" || detail.used == null) return null;
  const used = Number(detail.used);
  if (!Number.isFinite(used) || used < 0) return null;
  if (used === 0) {
    const zeroLimit = Number(detail.limit);
    return {
      ratio: 1,
      text: "100%",
      used: 0,
      total: Number.isFinite(zeroLimit) && zeroLimit > 0 ? zeroLimit : 0,
      resetAt: detail.resetTime || "",
    };
  }
  const limit = Number(detail.limit);
  if (!Number.isFinite(limit) || limit <= 0) return null;
  const remaining = limit - used;
  const ratio = clampRatio(remaining / limit);
  return {
    ratio,
    text: percentText(ratio),
    used,
    total: limit,
    resetAt: detail.resetTime || "",
  };
}

async function readBillingApi() {
  try {
    const response = await fetch("https://www.kimi.com/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages", {
      method: "POST",
      headers: {
        "content-type": "application/json",
        "x-msh-platform": "web",
        "x-msh-version": "1.0.0",
        "x-language": "zh-CN",
      },
      referrer: "https://www.kimi.com/code/console",
      referrerPolicy: "strict-origin-when-cross-origin",
      credentials: "include",
      body: JSON.stringify({ scope: ["FEATURE_CODING"] }),
    });

    if (!response.ok) {
      return null;
    }

    const payload = await response.json().catch(() => null);
    if (!payload) return null;
    const usages = Array.isArray(payload.usages) ? payload.usages : [];
    const usage = usages.find((item) => item && item.scope === "FEATURE_CODING") || usages[0];
    if (!usage) return null;

    const weekly = detailToQuota(usage.detail);
    const fiveHourDetail = Array.isArray(usage.limits) && usage.limits.length > 0
      ? (usage.limits[0].detail || usage.limits[0])
      : null;
    const fiveHour = detailToQuota(fiveHourDetail);

    if (!weekly && !fiveHour) return null;

    return { weekly, fiveHour };
  } catch {
    return null;
  }
}

function readBillingFromDoc(doc) {
  const token = doc.defaultView?.localStorage?.getItem("access_token");
  if (!token) return null;

  const request = new XMLHttpRequest();
  request.open("POST", "/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages", false);
  request.setRequestHeader("content-type", "application/json");
  request.setRequestHeader("authorization", "Bearer " + token);
  request.setRequestHeader("x-msh-platform", "web");
  request.setRequestHeader("x-msh-version", "1.0.0");
  request.setRequestHeader("x-language", "zh-CN");
  request.send(JSON.stringify({ scope: ["FEATURE_CODING"] }));

  if (request.status < 200 || request.status >= 300) return null;

  const payload = JSON.parse(request.responseText);
  const usages = Array.isArray(payload.usages) ? payload.usages : [];
  const usage = usages.find((item) => item && item.scope === "FEATURE_CODING") || usages[0];
  if (!usage) return null;

  const fiveHourDetail = Array.isArray(usage.limits) && usage.limits.length > 0
    ? (usage.limits[0].detail || usage.limits[0])
    : null;
  return {
    weekly: detailToQuota(usage.detail),
    fiveHour: detailToQuota(fiveHourDetail),
  };
}

function readKimiDomFallback(doc) {
  return {
    weekly: readTextQuota(doc, "本周用量"),
    fiveHour: readTextQuota(doc, "频限明细"),
  };
}
