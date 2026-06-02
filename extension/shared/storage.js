export const PLANS_STORAGE_KEY = "dde-coding-plan-plans";
export const QUOTA_CACHE_KEY = "dde-coding-plan-quota-cache";
export const REFRESH_STATE_KEY = "dde-coding-plan-refresh-state";

export const DEFAULT_PROVIDERS = [
  {
    id: "codex",
    name: "Codex / ChatGPT",
    quotaUrl: "https://chatgpt.com/codex/cloud/settings/analytics#usage",
    consoleUrl: "https://chatgpt.com/codex/cloud/settings/analytics#usage",
    loginUrl: "https://chatgpt.com/auth/login",
    allowedOrigin: "https://chatgpt.com",
  },
  {
    id: "kimi-code",
    name: "Kimi Code",
    quotaUrl: "https://www.kimi.com/code/console",
    consoleUrl: "https://www.kimi.com/code/console",
    loginUrl: "https://www.kimi.com/code/",
    allowedOrigin: "https://www.kimi.com",
  },
  {
    id: "glm-coding",
    name: "GLM Coding",
    quotaUrl: "https://bigmodel.cn/coding-plan/personal/usage",
    consoleUrl: "https://bigmodel.cn/coding-plan/personal/usage",
    loginUrl: "https://bigmodel.cn/",
    allowedOrigin: "https://bigmodel.cn",
  },
];

export async function getEnabledPlans() {
  const result = await chrome.storage.local.get(PLANS_STORAGE_KEY);
  const plans = result[PLANS_STORAGE_KEY];
  if (!plans || !Array.isArray(plans)) return DEFAULT_PROVIDERS.map((p) => p.id);
  return plans;
}

export async function setEnabledPlans(planIds) {
  await chrome.storage.local.set({ [PLANS_STORAGE_KEY]: planIds });
}

export async function addPlan(planId) {
  const plans = await getEnabledPlans();
  if (!plans.includes(planId)) {
    plans.push(planId);
    await setEnabledPlans(plans);
  }
}

export async function removePlan(planId) {
  const plans = await getEnabledPlans();
  const filtered = plans.filter((id) => id !== planId);
  await setEnabledPlans(filtered);
  const cache = await getQuotaCache();
  delete cache[planId];
  await chrome.storage.local.set({ [QUOTA_CACHE_KEY]: cache });
}

export async function getQuotaCache() {
  const result = await chrome.storage.local.get(QUOTA_CACHE_KEY);
  return result[QUOTA_CACHE_KEY] || {};
}

export async function setQuotaCacheEntry(providerId, data) {
  const cache = await getQuotaCache();
  cache[providerId] = {
    ...data,
    cachedAt: Date.now(),
  };
  await chrome.storage.local.set({ [QUOTA_CACHE_KEY]: cache });
}

export async function getRefreshState() {
  const result = await chrome.storage.local.get(REFRESH_STATE_KEY);
  return result[REFRESH_STATE_KEY] || { running: false, queue: [], current: null };
}

export async function setRefreshState(state) {
  await chrome.storage.local.set({ [REFRESH_STATE_KEY]: state });
}

export function isPlanExpired(cachedAt, ttlMs = 30 * 60 * 1000) {
  return !cachedAt || Date.now() - cachedAt > ttlMs;
}
