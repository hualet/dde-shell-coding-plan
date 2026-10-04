// SPDX-License-Identifier: GPL-3.0-or-later
import assert from "node:assert/strict";
import { codexProvider } from "../providers/codex.js";

function extract(text, search = "?tab=overview", pathname = "/settings/usage") {
  return codexProvider.extractQuota({ body: { innerText: text }, location: { pathname, search } });
}

// Captured from the new overview; later daily usage percentages are unrelated.
const overview = "套餐限额\n5 小时限额\n4 小时 35 分钟后重置\n剩余 94%\n每周限额\n6 天 11 小时后重置\n剩余 98%\n额度\n每日用量\nCLI\n58%";
assert.equal(extract(overview).raw?.fiveHour?.ratio, 0.94);
assert.equal(extract(overview).raw?.weekly?.ratio, 0.98);
assert.equal(extract("Plan limits\n5-hour limit\nResets in 4 hours\n94% remaining\nWeekly limit\n98% remaining").raw?.weekly?.ratio, 0.98);
assert.equal(extract("Codex\n5 小时限额\n周期\n已使用限额百分比\n28.6%\n每周限额\n6.9%", "?tab=analytics").success, false);
assert.equal(extract("5 小时限额\n加载中\n每周限额\n剩余 98%\n每日用量\n58%").raw?.fiveHour, null);
assert.equal(extract("每周限额\n加载中\n额度\n每日用量\n58%").success, false);
assert.equal(extract("Codex\n5 hour usage limit\n80%\nWeekly usage limit\n60%", "", "/codex/cloud/settings/analytics").raw?.weekly?.ratio, 0.6);
assert.equal(extract("5 小时限额\n剩余 0％\n每周限额\n剩余 100％").raw?.fiveHour?.ratio, 0);
assert.equal(codexProvider.quotaUrl, "https://chatgpt.com/settings/usage?tab=overview");
console.log("Codex extraction regression tests passed");
