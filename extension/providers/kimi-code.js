export const kimiCodeProvider = {
  id: "kimi-code",
  name: "Kimi Code",
  loginUrl: "https://www.kimi.com/code/",
  quotaUrl: "https://www.kimi.com/code/console",
  consoleUrl: "https://www.kimi.com/code/console",
  allowedOrigin: "https://www.kimi.com",
  loginIndicators: [".user-avatar", "[data-testid='avatar']"],

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
      parsed = readDomFallback(doc);
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
    const result = {
      providerId: "kimi-code",
      providerName: "Kimi Code",
      source: "browser_ext",
      status: "ok",
      updatedAt: new Date().toISOString(),
    };

    if (raw.weekly) {
      result.weeklyRemainingRatio = raw.weekly.ratio;
      result.weeklyBalanceText = raw.weekly.text;
      result.weeklyUsed = raw.weekly.used ?? -1;
      result.weeklyTotal = raw.weekly.total ?? -1;
      result.remainingRatio = raw.weekly.ratio;
      result.balanceText = raw.weekly.text;
      if (raw.weekly.used >= 0) result.used = raw.weekly.used;
      if (raw.weekly.total >= 0) result.total = raw.weekly.total;
      if (raw.weekly.resetAt) result.resetAt = raw.weekly.resetAt;
    }
    if (raw.fiveHour) {
      result.fiveHourRemainingRatio = raw.fiveHour.ratio;
      result.fiveHourBalanceText = raw.fiveHour.text;
    }

    if (!raw.weekly && !raw.fiveHour) {
      result.status = raw.status || "parse_error";
      result.message = raw.message || "Failed to parse quota";
    }

    return result;
  },
};

function clampRatio(value) {
  if (!Number.isFinite(value)) return -1;
  return Math.max(0, Math.min(1, value));
}

function percentText(ratio) {
  if (!Number.isFinite(ratio) || ratio < 0) return "";
  return Math.round(ratio * 100) + "%";
}

function detailToQuota(detail) {
  if (!detail) return null;
  const used = Number(detail.used);
  const limit = Number(detail.limit);
  if (!Number.isFinite(used) || !Number.isFinite(limit) || limit <= 0) return null;
  let remaining = Number(detail.remaining);
  if (!Number.isFinite(remaining)) remaining = limit - used;
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
  const response = await fetch("https://www.kimi.com/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages", {
    method: "POST",
    headers: {
      "content-type": "application/json",
      "x-msh-platform": "web",
      "x-msh-version": "1.0.0",
      "x-language": "zh-CN",
      "referer": "https://www.kimi.com/code/console",
      "origin": "https://www.kimi.com",
    },
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
  const fiveHour = detailToQuota(
    Array.isArray(usage.limits) && usage.limits.length > 0 ? usage.limits[0].detail : null
  );

  if (!weekly && !fiveHour) return null;

  return { weekly, fiveHour };
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

  return {
    weekly: detailToQuota(usage.detail),
    fiveHour: detailToQuota(
      Array.isArray(usage.limits) && usage.limits.length > 0 ? usage.limits[0].detail : null
    ),
  };
}

function readTextQuota(doc, label) {
  const text = doc.body ? doc.body.innerText : "";
  const labelIndex = text.indexOf(label);
  if (labelIndex < 0) return null;
  const section = text.slice(labelIndex, labelIndex + 200);
  const match = section.match(/(\d{1,3}(?:\.\d+)?)\s*%/);
  if (!match) return null;
  const usedRatio = clampRatio(Number(match[1]) / 100);
  const remainingRatio = usedRatio >= 0 ? 1 - usedRatio : -1;
  return {
    ratio: remainingRatio,
    text: percentText(remainingRatio),
  };
}

function readDomFallback(doc) {
  return {
    weekly: readTextQuota(doc, "本周用量"),
    fiveHour: readTextQuota(doc, "频限明细"),
  };
}
