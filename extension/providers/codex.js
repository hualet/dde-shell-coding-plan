export const codexProvider = {
  id: "codex",
  name: "Codex / ChatGPT",
  loginUrl: "https://chatgpt.com/auth/login",
  quotaUrl: "https://chatgpt.com/codex/cloud/settings/analytics#usage",
  consoleUrl: "https://chatgpt.com/codex/cloud/settings/analytics#usage",
  allowedOrigin: "https://chatgpt.com",
  loginIndicators: [".user-avatar", "[data-testid='profile-button']"],
  extractionMode: "tab",

  extractQuota(doc) {
    const text = (doc.body ? doc.body.innerText : "") || "";
    const normalized = text.replace(/\u00a0/g, " ").replace(/％/g, "%").replace(/[ \t]+/g, " ").trim();

    if (normalized.indexOf("Codex") < 0 && !doc.location?.pathname?.includes("/codex/cloud/settings/analytics")) {
      return { success: false, reason: "Not on Codex analytics page" };
    }

    const lines = normalized.split(/\n+/).map(l => l.trim()).filter(l => l.length > 0);

    let fiveHour = findQuotaInLines(lines, [/5\s*小时.*(使用|限额|限制|额度)/i, /5\s*hour.*(usage|limit)/i, /5-hour.*(usage|limit)/i]);
    let weekly = findQuotaInLines(lines, [/(每周|周).*(使用|限额|限制|额度)/i, /weekly.*(usage|limit)/i]);

    if (!fiveHour || !weekly) {
      const compact = normalized.replace(/\s+/g, "");
      if (!fiveHour && compact.indexOf("5小时") >= 0) {
        fiveHour = extractPercent(compact.slice(compact.indexOf("5小时"), compact.indexOf("5小时") + 240));
      }
    }

    if (!fiveHour && !weekly) {
      return { success: false, reason: "Could not find quota info on page" };
    }

    return {
      success: true,
      raw: { fiveHour, weekly },
    };
  },

  normalizeSnapshot(raw) {
    const result = {
      providerId: "codex",
      providerName: "Codex / ChatGPT",
      source: "browser_ext",
      status: "ok",
      updatedAt: new Date().toISOString(),
    };

    if (raw.weekly) {
      result.weeklyRemainingRatio = raw.weekly.ratio;
      result.weeklyBalanceText = raw.weekly.text;
      result.remainingRatio = raw.weekly.ratio;
      result.balanceText = raw.weekly.text;
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

function makeQuota(percentValue) {
  const remainingRatio = clampRatio(Number(percentValue) / 100);
  return {
    ratio: remainingRatio,
    text: percentText(remainingRatio),
  };
}

function extractPercent(section) {
  const match = section.match(/(\d{1,3}(?:\.\d+)?)\s*%/);
  if (!match) return null;
  return makeQuota(match[1]);
}

function findQuotaInLines(lines, patterns) {
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
