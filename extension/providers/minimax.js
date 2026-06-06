import { clampRatio, percentText, normalizeBodyText, splitIntoLines, findQuotaInLines, extractPercent, buildNormalizeSnapshot } from "../shared/quota-utils.js";

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
    const normalized = normalizeBodyText(doc);

    if (normalized.indexOf("MiniMax") < 0 && !doc.location?.hostname?.includes("minimaxi.com")) {
      return { success: false, reason: "Not on MiniMax billing page" };
    }

    const lines = splitIntoLines(normalized);

    let fiveHour = findQuotaInLines(lines, [/5\s*(?:小时|hour).*(?:使用|限额|限制|额度|usage|limit)/i]);
    let weekly = findQuotaInLines(lines, [/(?:每周|周).*(?:使用|限额|限制|额度)/i, /weekly.*(?:usage|limit)/i]);

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
    const adaptForMinimax = {};
    if (raw.weekly != null) {
      adaptForMinimax.weekly = {
        ratio: clampRatio(raw.weekly),
        text: percentText(clampRatio(raw.weekly)),
      };
    }
    if (raw.fiveHour != null) {
      adaptForMinimax.fiveHour = {
        ratio: clampRatio(raw.fiveHour),
        text: percentText(clampRatio(raw.fiveHour)),
      };
    }
    if (raw.status) adaptForMinimax.status = raw.status;
    if (raw.message) adaptForMinimax.message = raw.message;
    return buildNormalizeSnapshot("minimax", "MiniMax Coding", adaptForMinimax);
  },
};
