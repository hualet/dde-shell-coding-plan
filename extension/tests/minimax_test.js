import { minimaxProvider } from "../providers/minimax.js";

let passed = 0;
let failed = 0;

function assert(condition, message) {
  if (!condition) {
    console.error("FAIL: " + message);
    failed++;
  } else {
    passed++;
  }
}

function assertEqual(actual, expected, message) {
  if (actual !== expected) {
    console.error("FAIL: " + message + " — expected " + JSON.stringify(expected) + " got " + JSON.stringify(actual));
    failed++;
  } else {
    passed++;
  }
}

const { normalizeSnapshot } = minimaxProvider;

// 1. Empty object
{
  const r = normalizeSnapshot({});
  assertEqual(r.status, "parse_error", "empty object status");
  assertEqual(r.remainingRatio, undefined, "empty object remainingRatio");
  assertEqual(r.fiveHourRemainingRatio, undefined, "empty object fiveHourRemainingRatio");
  assertEqual(r.message, "Failed to parse quota", "empty object message");
  assertEqual(r.providerId, "minimax", "empty object providerId");
}

// 2. Both null
{
  const r = normalizeSnapshot({ weekly: null, fiveHour: null });
  assertEqual(r.status, "parse_error", "both null status");
}

// 3. Zero percent
{
  const r = normalizeSnapshot({ weekly: 0, fiveHour: 0 });
  assertEqual(r.status, "ok", "zero percent status");
  assertEqual(r.remainingRatio, 0, "zero percent remainingRatio");
  assertEqual(r.fiveHourRemainingRatio, 0, "zero percent fiveHourRemainingRatio");
  assertEqual(r.balanceText, "0%", "zero percent balanceText");
  assertEqual(r.fiveHourBalanceText, "0%", "zero percent fiveHourBalanceText");
}

// 4. Normal values
{
  const r = normalizeSnapshot({ weekly: 0.75, fiveHour: 0.5 });
  assertEqual(r.status, "ok", "normal status");
  assertEqual(r.remainingRatio, 0.75, "normal remainingRatio");
  assertEqual(r.fiveHourRemainingRatio, 0.5, "normal fiveHourRemainingRatio");
  assertEqual(r.balanceText, "75%", "normal balanceText");
  assertEqual(r.fiveHourBalanceText, "50%", "normal fiveHourBalanceText");
}

// 5. Out-of-bounds > 1
{
  const r = normalizeSnapshot({ weekly: 2.5, fiveHour: 1.5 });
  assertEqual(r.remainingRatio, 1, "oob >1 weekly clamped to 1");
  assertEqual(r.fiveHourRemainingRatio, 1, "oob >1 fiveHour clamped to 1");
}

// 6. Out-of-bounds < 0 (clamped to 0 by Math.max(0, ...))
{
  const r = normalizeSnapshot({ weekly: -3, fiveHour: -0.5 });
  assertEqual(r.remainingRatio, 0, "oob <0 weekly clamped to 0");
  assertEqual(r.fiveHourRemainingRatio, 0, "oob <0 fiveHour clamped to 0");
}

// 7. Non-numeric (NaN)
{
  const r = normalizeSnapshot({ weekly: NaN, fiveHour: NaN });
  assertEqual(r.status, "ok", "NaN status still ok (field present)");
  assertEqual(r.remainingRatio, -1, "NaN weekly -> -1");
  assertEqual(r.fiveHourRemainingRatio, -1, "NaN fiveHour -> -1");
}

// 8. Infinity
{
  const r = normalizeSnapshot({ weekly: Infinity, fiveHour: -Infinity });
  assertEqual(r.remainingRatio, -1, "Infinity weekly -> -1");
  assertEqual(r.fiveHourRemainingRatio, -1, "-Infinity fiveHour -> -1");
}

// 9. String value
{
  const r = normalizeSnapshot({ weekly: "abc", fiveHour: "50%" });
  assertEqual(r.remainingRatio, -1, "string weekly -> -1");
  assertEqual(r.fiveHourRemainingRatio, -1, "string fiveHour -> -1");
}

// 10. Only weekly, no fiveHour
{
  const r = normalizeSnapshot({ weekly: 0.6 });
  assertEqual(r.status, "ok", "weekly-only status");
  assertEqual(r.remainingRatio, 0.6, "weekly-only remainingRatio");
  assertEqual(r.fiveHourRemainingRatio, undefined, "weekly-only fiveHourRemainingRatio");
}

// 11. Only fiveHour, no weekly
{
  const r = normalizeSnapshot({ fiveHour: 0.3 });
  assertEqual(r.status, "ok", "fiveHour-only status");
  assertEqual(r.remainingRatio, undefined, "fiveHour-only remainingRatio");
  assertEqual(r.fiveHourRemainingRatio, 0.3, "fiveHour-only fiveHourRemainingRatio");
}

// 12. Custom error status/message
{
  const r = normalizeSnapshot({ status: "auth_error", message: "请登录" });
  assertEqual(r.status, "auth_error", "custom error status");
  assertEqual(r.message, "请登录", "custom error message");
}

console.log(`\nminimax normalizeSnapshot tests: ${passed} passed, ${failed} failed`);
if (failed > 0) {
  process.exit(1);
}
