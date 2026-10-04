import { normalizeBodyText, splitIntoLines, extractPercent, buildNormalizeSnapshot } from "../shared/quota-utils.js";

export const codexProvider = {
  id: "codex",
  name: "Codex / ChatGPT",
  loginUrl: "https://chatgpt.com/auth/login",
  quotaUrl: "https://chatgpt.com/settings/usage?tab=overview",
  consoleUrl: "https://chatgpt.com/settings/usage?tab=overview",
  allowedOrigin: "https://chatgpt.com",
  loginIndicators: [".user-avatar", "[data-testid='profile-button']"],
  extractionMode: "tab",

  extractQuota(doc) {
    const normalized = normalizeBodyText(doc);

    const isUsagePage = doc.location?.pathname === "/settings/usage";
    if (isUsagePage && new URLSearchParams(doc.location?.search || "").get("tab") === "analytics") {
      return { success: false, reason: "Usage analytics contains historical usage, not current quota" };
    }
    if (!isUsagePage && normalized.indexOf("Codex") < 0 && !doc.location?.pathname?.includes("/codex/cloud/settings/analytics")) {
      return { success: false, reason: "Not on Codex usage page" };
    }

    const lines = splitIntoLines(normalized);
    const fiveHourPattern = /^(?:5\s*小时.*(?:使用|限额|限制|额度)|5[\s-]*hour.*(?:usage|limit))/i;
    const weeklyPattern = /^(?:(?:每周|周).*(?:使用|限额|限制|额度)|weekly.*(?:usage|limit))/i;

    // Bound each quota to its section. Never consume another quota or usage history.
    function readQuota(pattern) {
      const start = lines.findIndex(line => pattern.test(line));
      if (start < 0) return null;
      for (let i = start; i < Math.min(start + 8, lines.length); ++i) {
        const line = lines[i];
        if (i > start && (fiveHourPattern.test(line) || weeklyPattern.test(line) || /^(?:额度|每日用量|套餐用量历史|credits|daily usage|plan usage history)/i.test(line))) break;
        if (isUsagePage && !/(?:剩余|remaining)/i.test(line)) continue;
        const quota = extractPercent(line);
        if (quota) return quota;
      }
      return null;
    }

    const fiveHour = readQuota(fiveHourPattern);
    const weekly = readQuota(weeklyPattern);

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
