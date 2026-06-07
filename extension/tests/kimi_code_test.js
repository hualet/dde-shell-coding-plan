import { kimiCodeProvider, detailToQuota } from "../providers/kimi-code.js";
import { assert, assertEqual, printSummary } from "./test-helpers.js";

// --- detailToQuota unit tests ---

// 1. Core regression: used=20, limit=100, remaining=20 => ratio=0.8
//    Even if API returns remaining=20 (wrong: equal to used), we compute limit - used = 80, ratio=0.8
{
  const q = detailToQuota({ used: 20, limit: 100, remaining: 20 });
  assert(q !== null, "detailToQuota should return non-null for valid input");
  assertEqual(q.ratio, 0.8, "used=20,limit=100,remaining=20 => ratio must be 0.8 (not 0.2)");
  assertEqual(q.text, "80%", "used=20,limit=100 => text must be 80%");
  assertEqual(q.used, 20, "used must be 20");
  assertEqual(q.total, 100, "total must be 100");
}

// 2. detailToQuota ignores API remaining field even when it differs
{
  const q = detailToQuota({ used: 30, limit: 100, remaining: 999 });
  assertEqual(q.ratio, 0.7, "used=30,limit=100,remaining=999 => ratio must be 0.7 (ignores remaining=999)");
}

// 3. detailToQuota: no remaining field at all
{
  const q = detailToQuota({ used: 50, limit: 200 });
  assertEqual(q.ratio, 0.75, "used=50,limit=200,no remaining => ratio=0.75");
  assertEqual(q.text, "75%", "used=50,limit=200 => text=75%");
}

// 4. detailToQuota: fully used
{
  const q = detailToQuota({ used: 100, limit: 100, remaining: 0 });
  assertEqual(q.ratio, 0, "fully used => ratio=0");
}

// 5. detailToQuota: nothing used, normal limit
{
  const q = detailToQuota({ used: 0, limit: 100, remaining: 100 });
  assertEqual(q.ratio, 1, "nothing used => ratio=1");
  assertEqual(q.text, "100%", "nothing used => text=100%");
  assertEqual(q.used, 0, "nothing used => used=0");
  assertEqual(q.total, 100, "nothing used => total=100");
}

// 5b. KEY REGRESSION: used=0, limit=0 (API returns zero limit when nothing used)
//     Should return ratio=1 (fully available), NOT null
{
  const q = detailToQuota({ used: 0, limit: 0 });
  assert(q !== null, "used=0,limit=0 => must NOT return null");
  assertEqual(q.ratio, 1, "used=0,limit=0 => ratio must be 1 (fully available)");
  assertEqual(q.text, "100%", "used=0,limit=0 => text must be 100%");
  assertEqual(q.used, 0, "used=0,limit=0 => used must be 0");
}

// 5c. KEY REGRESSION: used=0, no limit field at all (API omits limit when nothing used)
//     Should return ratio=1 (fully available), NOT null
{
  const q = detailToQuota({ used: 0 });
  assert(q !== null, "used=0,no limit => must NOT return null");
  assertEqual(q.ratio, 1, "used=0,no limit => ratio must be 1 (fully available)");
  assertEqual(q.text, "100%", "used=0,no limit => text must be 100%");
}

// 5d. used=0, limit=undefined
{
  const q = detailToQuota({ used: 0, limit: undefined });
  assert(q !== null, "used=0,limit=undefined => must NOT return null");
  assertEqual(q.ratio, 1, "used=0,limit=undefined => ratio=1");
}

// 6. detailToQuota: null detail returns null
{
  const q = detailToQuota(null);
  assertEqual(q, null, "null detail returns null");
}

// 7. detailToQuota: zero limit with positive used returns null
{
  const q = detailToQuota({ used: 10, limit: 0 });
  assertEqual(q, null, "positive used, zero limit returns null");
}

// 8. detailToQuota: resetTime passed through
{
  const q = detailToQuota({ used: 10, limit: 100, resetTime: "2026-06-05T00:00:00Z" });
  assertEqual(q.resetAt, "2026-06-05T00:00:00Z", "resetTime passed through");
}

// --- normalizeSnapshot tests ---

const { normalizeSnapshot } = kimiCodeProvider;

// 9. Empty raw
{
  const r = normalizeSnapshot({});
  assertEqual(r.status, "parse_error", "empty raw status");
  assertEqual(r.providerId, "kimi-code", "empty raw providerId");
  assertEqual(r.source, "browser_ext", "empty raw source");
}

// 10. Both null
{
  const r = normalizeSnapshot({ weekly: null, fiveHour: null });
  assertEqual(r.status, "parse_error", "both null status");
}

// 11. Weekly + fiveHour with ratio/used/total
{
  const r = normalizeSnapshot({
    weekly: { ratio: 0.6, text: "60%", used: 40, total: 100, resetAt: "" },
    fiveHour: { ratio: 0.8, text: "80%", used: 20, total: 100, resetAt: "" },
  });
  assertEqual(r.status, "ok", "both present status");
  assertEqual(r.weeklyRemainingRatio, 0.6, "weekly remainingRatio");
  assertEqual(r.weeklyBalanceText, "60%", "weekly balanceText");
  assertEqual(r.weeklyUsed, 40, "weekly used");
  assertEqual(r.weeklyTotal, 100, "weekly total");
  assertEqual(r.fiveHourRemainingRatio, 0.8, "fiveHour remainingRatio");
  assertEqual(r.fiveHourBalanceText, "80%", "fiveHour balanceText");
  assertEqual(r.remainingRatio, 0.6, "primary remainingRatio is weekly");
  assertEqual(r.balanceText, "60%", "primary balanceText is weekly");
  assertEqual(r.used, 40, "primary used from weekly");
  assertEqual(r.total, 100, "primary total from weekly");
}

// 12. Only weekly, no fiveHour
{
  const r = normalizeSnapshot({
    weekly: { ratio: 0.5, text: "50%", used: 50, total: 100, resetAt: "" },
  });
  assertEqual(r.status, "ok", "weekly-only status");
  assertEqual(r.weeklyRemainingRatio, 0.5, "weekly-only weekly remaining");
  assertEqual(r.remainingRatio, 0.5, "weekly-only primary remaining");
  assertEqual(r.source, "browser_ext", "weekly-only source");
}

// 13. Only fiveHour, no weekly
{
  const r = normalizeSnapshot({
    fiveHour: { ratio: 0.3, text: "30%", used: 70, total: 100, resetAt: "" },
  });
  assertEqual(r.status, "ok", "fiveHour-only status");
  assertEqual(r.fiveHourRemainingRatio, 0.3, "fiveHour-only fiveHour remaining");
}

// 14. Source is always browser_ext, never webview
{
  const r = normalizeSnapshot({
    weekly: { ratio: 0.9, text: "90%", used: 10, total: 100, resetAt: "" },
  });
  assertEqual(r.source, "browser_ext", "source must be browser_ext");
  assert(r.source !== "webview", "source must not be webview");
}

// 15. Provider ID is kimi-code
{
  const r = normalizeSnapshot({
    weekly: { ratio: 0.1, text: "10%", used: 90, total: 100, resetAt: "" },
  });
  assertEqual(r.providerId, "kimi-code", "providerId is kimi-code");
}

// 16. Zero remaining (fully used)
{
  const r = normalizeSnapshot({
    weekly: { ratio: 0, text: "0%", used: 100, total: 100, resetAt: "" },
    fiveHour: { ratio: 0, text: "0%", used: 50, total: 50, resetAt: "" },
  });
  assertEqual(r.weeklyRemainingRatio, 0, "zero weekly remaining");
  assertEqual(r.fiveHourRemainingRatio, 0, "zero fiveHour remaining");
  assertEqual(r.weeklyUsed, 100, "zero weekly used");
  assertEqual(r.weeklyTotal, 100, "zero weekly total");
}

// 17. Custom error status/message
{
  const r = normalizeSnapshot({ status: "auth_error", message: "请登录" });
  assertEqual(r.status, "auth_error", "custom error status");
  assertEqual(r.message, "请登录", "custom error message");
}

// --- Strict type validation: used must be a real number === 0 ---

// 18. used=null must return null (Number(null)===0 is NOT accepted)
{
  const q = detailToQuota({ used: null, limit: 100 });
  assertEqual(q, null, "used=null must return null");
}

// 19. used="" must return null (Number("")===0 is NOT accepted)
{
  const q = detailToQuota({ used: "", limit: 100 });
  assertEqual(q, null, "used='' must return null");
}

// 20. used="abc" must return null
{
  const q = detailToQuota({ used: "abc", limit: 100 });
  assertEqual(q, null, "used='abc' must return null");
}

// 21. used=undefined must return null
{
  const q = detailToQuota({ limit: 100 });
  assertEqual(q, null, "used=undefined must return null");
}

// 22. used=-1 (negative) must return null
{
  const q = detailToQuota({ used: -1, limit: 100 });
  assertEqual(q, null, "used=-1 must return null");
}

// 23. used=NaN must return null
{
  const q = detailToQuota({ used: NaN, limit: 100 });
  assertEqual(q, null, "used=NaN must return null");
}

// 24. used=Infinity must return null
{
  const q = detailToQuota({ used: Infinity, limit: 100 });
  assertEqual(q, null, "used=Infinity must return null");
}

// 25. used="0" (string zero) from the live Kimi API is accepted
{
  const q = detailToQuota({ used: "0", limit: 100 });
  assert(q !== null, "used='0',limit=100 must return non-null");
  if (q) {
    assertEqual(q.ratio, 1, "used='0',limit=100 => ratio=1");
    assertEqual(q.text, "100%", "used='0',limit=100 => text=100%");
    assertEqual(q.used, 0, "used='0',limit=100 => used=0");
    assertEqual(q.total, 100, "used='0',limit=100 => total=100");
  }
}

// 26. Explicit 1%: used=1, limit=100, remaining=1 => ratio=0.99
{
  const q = detailToQuota({ used: 1, limit: 100, remaining: 1 });
  assert(q !== null, "used=1,limit=100 must return non-null");
  assertEqual(q.ratio, 0.99, "used=1,limit=100 => ratio=0.99");
  assertEqual(q.text, "99%", "used=1,limit=100 => text=99%");
}

// 27. used and limit are numeric strings from the live Kimi API
{
  const q = detailToQuota({ used: "50", limit: "100", remaining: "50" });
  assert(q !== null, "used='50',limit='100' must return non-null");
  if (q) {
    assertEqual(q.ratio, 0.5, "used='50',limit='100' => ratio=0.5");
    assertEqual(q.text, "50%", "used='50',limit='100' => text=50%");
    assertEqual(q.used, 50, "used='50',limit='100' => used=50");
    assertEqual(q.total, 100, "used='50',limit='100' => total=100");
  }
}

// 28. Limits array element without .detail wrapper (API may return used/limit directly)
{
  const q = detailToQuota({ used: 10, limit: 50 });
  assert(q !== null, "limit object without .detail wrapper must return non-null");
  if (q) {
    assertEqual(q.ratio, 0.8, "direct limit object => ratio=0.8");
    assertEqual(q.text, "80%", "direct limit object => text=80%");
    assertEqual(q.used, 10, "direct limit object => used=10");
    assertEqual(q.total, 50, "direct limit object => total=50");
  }
}

printSummary("kimi-code tests");
