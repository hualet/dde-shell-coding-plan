export const minimaxProvider = {
  id: "minimax",
  name: "MiniMax Coding",
  loginUrl: "https://platform.minimaxi.com/",
  quotaUrl: "https://platform.minimaxi.com/user-center/billing",
  consoleUrl: "https://platform.minimaxi.com/user-center/billing",
  allowedOrigin: "https://platform.minimaxi.com",
  loginIndicators: [".user-info", "[data-testid='user-avatar']"],
  extractionMode: "tab",

  extractQuota(doc) {
    const text = (doc.body ? doc.body.innerText : "") || "";
    const normalized = text.replace(/\u00a0/g, " ").replace(/％/g, "%").replace(/[ \t]+/g, " ").trim();

    if (normalized.indexOf("MiniMax") < 0 && !doc.location?.hostname?.includes("minimaxi.com")) {
      return { success: false, reason: "Not on MiniMax billing page" };
    }

    const lines = normalized.split(/\n+/).map(l => l.trim()).filter(l => l.length > 0);

    let fiveHour = null;
    let weekly = null;

    for (let i = 0; i < lines.length; ++i) {
      if (/5\s*(?:小时|hour).*(?:使用|限额|限制|额度|usage|limit)/i.test(lines[i])) {
        for (let o = 1; o <= 6 && i + o < lines.length; ++o) {
          const m = lines[i + o].match(/^(\d{1,3}(?:\.\d+)?)\s*(?:%|％)$/);
          if (m) { fiveHour = Number(m[1]) / 100; break; }
        }
        if (fiveHour == null) {
          const sec = lines.slice(i, i + 8).join("\n");
          const pm = sec.match(/(\d{1,3}(?:\.\d+)?)\s*%/);
          if (pm) fiveHour = Number(pm[1]) / 100;
        }
      }
      if (/(?:每周|周).*(?:使用|限额|限制|额度)/i.test(lines[i]) || /weekly.*(?:usage|limit)/i.test(lines[i])) {
        for (let o = 1; o <= 6 && i + o < lines.length; ++o) {
          const m = lines[i + o].match(/^(\d{1,3}(?:\.\d+)?)\s*(?:%|％)$/);
          if (m) { weekly = Number(m[1]) / 100; break; }
        }
        if (weekly == null) {
          const sec = lines.slice(i, i + 8).join("\n");
          const pm = sec.match(/(\d{1,3}(?:\.\d+)?)\s*%/);
          if (pm) weekly = Number(pm[1]) / 100;
        }
      }
    }

    if (fiveHour == null && weekly == null) {
      return {
        success: false,
        reason: "Could not find MiniMax Coding quota info",
      };
    }

    return {
      success: true,
      raw: { fiveHour, weekly },
    };
  },

  normalizeSnapshot(raw) {
    const clamp = (v) => (Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : -1);
    const pct = (v) => (v >= 0 ? Math.round(v * 100) + "%" : "");

    const result = {
      providerId: "minimax",
      providerName: "MiniMax Coding",
      source: "browser_ext",
      status: "ok",
      updatedAt: new Date().toISOString(),
    };

    if (raw.weekly != null) {
      result.weeklyRemainingRatio = clamp(raw.weekly);
      result.weeklyBalanceText = pct(clamp(raw.weekly));
      result.remainingRatio = clamp(raw.weekly);
      result.balanceText = pct(clamp(raw.weekly));
    }
    if (raw.fiveHour != null) {
      result.fiveHourRemainingRatio = clamp(raw.fiveHour);
      result.fiveHourBalanceText = pct(clamp(raw.fiveHour));
    }

    if (raw.weekly == null && raw.fiveHour == null) {
      result.status = raw.status || "parse_error";
      result.message = raw.message || "Failed to parse quota";
    }

    return result;
  },
};
