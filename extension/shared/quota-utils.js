export function clampRatio(value) {
  if (!Number.isFinite(value)) return -1;
  return Math.max(0, Math.min(1, value));
}

export function percentText(ratio) {
  if (!Number.isFinite(ratio) || ratio < 0) return "";
  return Math.round(ratio * 100) + "%";
}

export function makeQuota(percentValue) {
  const remainingRatio = clampRatio(Number(percentValue) / 100);
  return {
    ratio: remainingRatio,
    text: percentText(remainingRatio),
  };
}

export function extractPercent(section) {
  const match = section.match(/(\d{1,3}(?:\.\d+)?)\s*%/);
  if (!match) return null;
  return makeQuota(match[1]);
}

export function findQuotaInLines(lines, patterns) {
  for (let i = 0; i < lines.length; ++i) {
    const matchesPattern = patterns.some(p => p.test(lines[i]));
    if (!matchesPattern) continue;

    for (let offset = 1; offset <= 6 && i + offset < lines.length; ++offset) {
      const value = lines[i + offset].match(/^(\d{1,3}(?:\.\d+)?)\s*(?:%|％)$/);
      if (value) return makeQuota(value[1]);
    }

    const section = lines.slice(i, i + 8).join("\n");
    const quota = extractPercent(section);
    if (quota) return quota;
  }
  return null;
}

export function normalizeBodyText(doc) {
  const text = (doc.body ? doc.body.innerText : "") || "";
  return text.replace(/\u00a0/g, " ").replace(/％/g, "%").replace(/[ \t]+/g, " ").trim();
}

export function splitIntoLines(normalized) {
  return normalized.split(/\n+/).map(l => l.trim()).filter(l => l.length > 0);
}

export function readTextQuota(doc, label) {
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

export function buildNormalizeSnapshot(providerId, providerName, raw, options = {}) {
  const { includeUsage } = options;
  const result = {
    providerId,
    providerName,
    source: "browser_ext",
    status: "ok",
    updatedAt: new Date().toISOString(),
  };

  if (raw.weekly) {
    result.weeklyRemainingRatio = raw.weekly.ratio;
    result.weeklyBalanceText = raw.weekly.text;
    result.remainingRatio = raw.weekly.ratio;
    result.balanceText = raw.weekly.text;
    if (includeUsage) {
      result.weeklyUsed = raw.weekly.used ?? -1;
      result.weeklyTotal = raw.weekly.total ?? -1;
      if (raw.weekly.used >= 0) result.used = raw.weekly.used;
      if (raw.weekly.total >= 0) result.total = raw.weekly.total;
      if (raw.weekly.resetAt) result.resetAt = raw.weekly.resetAt;
    }
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
}
