import { normalizeBodyText, splitIntoLines, findQuotaInLines, extractPercent, buildNormalizeSnapshot } from "../shared/quota-utils.js";

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
    const normalized = normalizeBodyText(doc);

    if (normalized.indexOf("Codex") < 0 && !doc.location?.pathname?.includes("/codex/cloud/settings/analytics")) {
      return { success: false, reason: "Not on Codex analytics page" };
    }

    const lines = splitIntoLines(normalized);

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
    return buildNormalizeSnapshot("codex", "Codex / ChatGPT", raw);
  },
};
