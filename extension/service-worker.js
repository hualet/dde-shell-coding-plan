import { buildProviderMap, getTabProviderIds } from "./providers/index.js";

import {
  MSG_TYPE_AUTH,
  MSG_TYPE_STATUS,
  MSG_TYPE_REFRESH_REQUEST,
  MSG_TYPE_REFRESH_RESULT,
  MSG_TYPE_REFRESH_PROGRESS,
  MSG_TYPE_OPEN_CONSOLE,
  MSG_TYPE_HEARTBEAT,
  WS_URL,
  HEARTBEAT_INTERVAL_MS,
  ALARM_NAME,
  ALARM_INTERVAL_MINUTES,
  TOKEN_STORAGE_KEY,
  STATUS_STORAGE_KEY,
  AUTO_REFRESH_ALARM,
  AUTO_REFRESH_INTERVAL_MINUTES,
} from "./shared/ws-protocol.js";

import {
  getEnabledPlans,
  setQuotaCacheEntry,
  PLANS_STORAGE_KEY,
} from "./shared/storage.js";

const PROVIDERS = buildProviderMap();
const TAB_PROVIDER_IDS = getTabProviderIds();

let ws = null;
let authenticated = false;
let heartbeatTimer = null;
let connectTimeoutTimer = null;

const refreshQueue = [];
let refreshRunning = false;

function log(...args) {
  console.log("[dde-coding-plan]", ...args);
}

function warn(...args) {
  console.warn("[dde-coding-plan]", ...args);
}

function error(...args) {
  console.error("[dde-coding-plan]", ...args);
}

function updateConnectionStatus(status) {
  chrome.storage.local.set({ [STATUS_STORAGE_KEY]: status });
}

function sendJson(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(msg));
    log("send:", msg.type, msg);
  } else {
    warn("cannot send, ws not open");
  }
}

async function sendStatus() {
  const availableProviders = await getEnabledPlans();
  sendJson({
    type: MSG_TYPE_STATUS,
    connected: true,
    availableProviders,
  });
}

function startHeartbeat() {
  stopHeartbeat();
  heartbeatTimer = setInterval(() => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      sendJson({ type: MSG_TYPE_HEARTBEAT });
    }
  }, HEARTBEAT_INTERVAL_MS);
}

function stopHeartbeat() {
  if (heartbeatTimer) {
    clearInterval(heartbeatTimer);
    heartbeatTimer = null;
  }
}

async function getToken() {
  const result = await chrome.storage.local.get(TOKEN_STORAGE_KEY);
  return result[TOKEN_STORAGE_KEY] || "";
}

// Connects to the native WebSocket server at ws://127.0.0.1:18765
// Handles message types: auth, refresh_request, refresh_result, refresh_progress, heartbeat
async function connect() {
  if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
    log("connect: already connected or connecting");
    return;
  }

  const token = await getToken();
  if (!token) {
    warn("connect: no token configured");
    updateConnectionStatus("no_token");
    return;
  }

  log("connect: connecting to", WS_URL);
  updateConnectionStatus("connecting");

  if (connectTimeoutTimer) {
    clearTimeout(connectTimeoutTimer);
  }

  try {
    ws = new WebSocket(WS_URL);
  } catch (err) {
    error("connect: WebSocket constructor failed:", err);
    updateConnectionStatus("error");
    return;
  }

  connectTimeoutTimer = setTimeout(() => {
    if (ws && ws.readyState !== WebSocket.OPEN) {
      warn("connect: connection timeout, closing");
      ws.close();
      ws = null;
      updateConnectionStatus("timeout");
    }
  }, 10000);

  ws.onopen = () => {
    log("connect: WebSocket onopen");
    if (connectTimeoutTimer) {
      clearTimeout(connectTimeoutTimer);
      connectTimeoutTimer = null;
    }
    updateConnectionStatus("authenticating");
    sendJson({ type: MSG_TYPE_AUTH, token });
  };

  ws.onmessage = (event) => {
    let msg;
    try {
      msg = JSON.parse(event.data);
    } catch {
      warn("onmessage: invalid JSON:", event.data);
      return;
    }
    log("onmessage:", msg.type, msg);
    handleMessage(msg);
  };

  ws.onclose = () => {
    log("connect: WebSocket onclose");
    if (connectTimeoutTimer) {
      clearTimeout(connectTimeoutTimer);
      connectTimeoutTimer = null;
    }
    stopHeartbeat();
    if (authenticated) {
      authenticated = false;
      updateConnectionStatus("disconnected");
    } else {
      updateConnectionStatus("auth_failed");
    }
    ws = null;
  };

  ws.onerror = (err) => {
    error("connect: WebSocket onerror:", err.message || err);
    if (connectTimeoutTimer) {
      clearTimeout(connectTimeoutTimer);
      connectTimeoutTimer = null;
    }
    updateConnectionStatus("error");
  };
}

function handleMessage(msg) {
  const { type } = msg;

  if (type === "auth_result") {
    handleAuthResult(msg);
  } else if (type === MSG_TYPE_REFRESH_REQUEST) {
    handleRefreshRequest(msg).catch((err) => error("handleRefreshRequest failed", err));
  } else if (type === MSG_TYPE_OPEN_CONSOLE) {
    handleOpenConsole(msg);
  } else {
    warn("handleMessage: unknown type:", type);
  }
}

function handleAuthResult(msg) {
  if (msg.success) {
    authenticated = true;
    log("handleAuthResult: auth success");
    updateConnectionStatus("connected");
    startHeartbeat();
    sendStatus().catch((err) => error("sendStatus failed", err));
  } else {
    authenticated = false;
    warn("handleAuthResult: auth failed:", msg.message);
    updateConnectionStatus("auth_failed");
  }
}

function enqueueRefresh(providerIds, wsRequestId, wsTimeout) {
  for (const pid of providerIds) {
    refreshQueue.push({ providerId: pid, wsRequestId: wsRequestId || null, wsTimeout: wsTimeout || 20000 });
  }
  log("enqueueRefresh: queue size now", refreshQueue.length, "running:", refreshRunning);
  if (!refreshRunning) {
    drainRefreshQueue();
  }
}

async function drainRefreshQueue() {
  if (refreshRunning) return;
  refreshRunning = true;

  while (refreshQueue.length > 0) {
    const item = refreshQueue.shift();
    const { providerId, wsRequestId, wsTimeout } = item;
    const provider = PROVIDERS[providerId];

    if (!provider) {
      warn("drainRefreshQueue: unknown provider:", providerId);
      if (wsRequestId) {
        sendJson({
          type: MSG_TYPE_REFRESH_RESULT,
          requestId: wsRequestId,
          provider: providerId,
          data: {
            providerId,
            providerName: providerId,
            source: "browser_ext",
            status: "parse_error",
            message: "Unknown provider: " + providerId,
            updatedAt: new Date().toISOString(),
          },
        });
      }
      continue;
    }

    if (wsRequestId) {
      sendJson({
        type: MSG_TYPE_REFRESH_PROGRESS,
        requestId: wsRequestId,
        provider: providerId,
        status: "loading",
        message: "正在加载额度页面...",
      });
    }

    try {
      log("drainRefreshQueue: extracting", providerId);
      const result = await extractProviderQuota(provider, wsTimeout);
      const data = buildCacheData(provider, result);

      if (result._debug) {
        log("drainRefreshQueue:", providerId, "debug:", JSON.stringify(result._debug));
      }
      log("drainRefreshQueue:", providerId, "data: weeklyRatio=", data.weeklyRemainingRatio, "fiveHourRatio=", data.fiveHourRemainingRatio, "balanceText=", data.weeklyBalanceText, "fiveHourText=", data.fiveHourBalanceText);

      if (wsRequestId) {
        const { _debug, ...wsData } = data;
        sendJson({
          type: MSG_TYPE_REFRESH_RESULT,
          requestId: wsRequestId,
          provider: providerId,
          data: wsData,
        });
      }

      await setQuotaCacheEntry(providerId, data);
      log("drainRefreshQueue: done", providerId, "status:", result.status);
    } catch (err) {
      error("drainRefreshQueue: failed for", providerId, err.message);
      const errData = buildErrorCacheData(provider, err.message);

      if (wsRequestId) {
        sendJson({
          type: MSG_TYPE_REFRESH_RESULT,
          requestId: wsRequestId,
          provider: providerId,
          data: errData,
        });
      }

      await setQuotaCacheEntry(providerId, errData);
    }
  }

  refreshRunning = false;
  log("drainRefreshQueue: queue drained");
}

function buildCacheData(provider, result) {
  return {
    providerId: provider.id,
    providerName: provider.name,
    source: "browser_ext",
    status: result.status,
    remainingRatio: result.remainingRatio ?? -1,
    weeklyRemainingRatio: result.weeklyRemainingRatio ?? result.remainingRatio ?? -1,
    weeklyBalanceText: result.weeklyBalanceText || result.balanceText || "",
    fiveHourRemainingRatio: result.fiveHourRemainingRatio ?? -1,
    fiveHourBalanceText: result.fiveHourBalanceText || "",
    balanceText: result.balanceText || "",
    used: result.used ?? result.weeklyUsed ?? -1,
    total: result.total ?? result.weeklyTotal ?? -1,
    unit: result.unit || "credit",
    updatedAt: new Date().toISOString(),
    consoleUrl: provider.consoleUrl || "",
    message: result.message || "",
    _debug: result._debug || undefined,
  };
}

function buildErrorCacheData(provider, errMsg) {
  const isAuthError = typeof errMsg === "string" && errMsg.startsWith("auth_error:");
  const message = isAuthError ? errMsg.replace("auth_error:", "") : (errMsg || "刷新失败");
  return {
    providerId: provider.id,
    providerName: provider.name,
    source: "browser_ext",
    status: isAuthError ? "auth_error" : "network_error",
    message,
    updatedAt: new Date().toISOString(),
  };
}

async function handleRefreshRequest(msg) {
  const { requestId, providers: providerIds, timeout } = msg;
  log("handleRefreshRequest:", requestId, providerIds, "timeout:", timeout);

  if (!authenticated) {
    warn("handleRefreshRequest: not authenticated");
    return;
  }

  const enabled = await getEnabledPlans();
  const filtered = providerIds.filter((id) => enabled.includes(id));
  if (filtered.length === 0) {
    log("handleRefreshRequest: no enabled providers after intersection");
    return;
  }

  enqueueRefresh(filtered, requestId, timeout || 15000);
}

function extractProviderQuota(provider, timeout) {
  if (TAB_PROVIDER_IDS.has(provider.id)) {
    return extractViaTab(provider, timeout);
  }
  return extractViaOffscreen(provider, timeout);
}

function extractViaTab(provider, timeout) {
  return new Promise((resolve, reject) => {
    let settled = false;
    let tabId = null;
    let updatedListener = null;
    let removedListener = null;

    function cleanup() {
      clearTimeout(timer);
      if (updatedListener) {
        chrome.tabs.onUpdated.removeListener(updatedListener);
        updatedListener = null;
      }
      if (removedListener) {
        chrome.tabs.onRemoved.removeListener(removedListener);
        removedListener = null;
      }
      if (tabId !== null) {
        const id = tabId;
        tabId = null;
        chrome.tabs.remove(id).catch(() => {});
      }
    }

    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      error("extractViaTab: timeout for", provider.id);
      cleanup();
      reject(new Error("Timeout extracting quota for " + provider.id));
    }, timeout);

    chrome.tabs.create({ url: provider.quotaUrl, active: false }, (tab) => {
      if (chrome.runtime.lastError) {
        if (settled) return;
        settled = true;
        cleanup();
        reject(new Error(chrome.runtime.lastError.message));
        return;
      }

      tabId = tab.id;

      removedListener = function (removedTabId) {
        if (removedTabId !== tabId) return;
        if (settled) return;
        settled = true;
        cleanup();
        reject(new Error("Tab was closed before extraction completed"));
      };
      chrome.tabs.onRemoved.addListener(removedListener);

      updatedListener = function (updatedTabId, changeInfo) {
        if (updatedTabId !== tabId || changeInfo.status !== "complete") return;
        chrome.tabs.onUpdated.removeListener(updatedListener);
        updatedListener = null;

        setTimeout(async () => {
          if (settled) {
            cleanup();
            return;
          }

          try {
            const results = await chrome.scripting.executeScript({
              target: { tabId },
              func: extractQuotaInPage,
              args: [provider.id],
            });

            let lastResult = results?.[0]?.result;
            if (lastResult && lastResult.success) {
              if (settled) { cleanup(); return; }
              settled = true;
              cleanup();
              log("extractViaTab: success for", provider.id);
              resolve(lastResult.data);
              return;
            }

            if (!lastResult || lastResult.needsRetry) {
              for (let attempt = 1; attempt <= 4; attempt++) {
                await new Promise(r => setTimeout(r, 1500));
                if (settled) { cleanup(); return; }

                const retryResults = await chrome.scripting.executeScript({
                  target: { tabId },
                  func: extractQuotaInPage,
                  args: [provider.id],
                });

                lastResult = retryResults?.[0]?.result;
                if (lastResult && lastResult.success) {
                  if (settled) { cleanup(); return; }
                  settled = true;
                  cleanup();
                  log("extractViaTab: success for", provider.id, "attempt", attempt + 1);
                  resolve(lastResult.data);
                  return;
                }
                if (!lastResult?.needsRetry) break;
              }
            }

            if (settled) { cleanup(); return; }
            settled = true;
            cleanup();
            const errMsg = lastResult?.error || "Extraction failed in tab";
            error("extractViaTab: failed for", provider.id, errMsg);
            reject(new Error(errMsg));
          } catch (err) {
            if (settled) { cleanup(); return; }
            settled = true;
            cleanup();
            error("extractViaTab: executeScript error for", provider.id, err.message);
            reject(new Error(err.message));
          }
        }, 1500);
      };

      chrome.tabs.onUpdated.addListener(updatedListener);
    });
  });
}

function extractQuotaInPage(providerId) {
  const clamp = (v) => (Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : -1);
  const pct = (v) => (v >= 0 ? Math.round(v * 100) + "%" : "");

  const PROVIDERS = {
    codex: {
      providerId: "codex",
      providerName: "Codex / ChatGPT",
      extract(doc) {
        const text = (doc.body ? doc.body.innerText : "") || "";
        const normalized = text.replace(/\u00a0/g, " ").replace(/％/g, "%").replace(/[ \t]+/g, " ").trim();
        if (normalized.indexOf("Codex") < 0 && !doc.location?.pathname?.includes("/codex/cloud/settings/analytics")) {
          return null;
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
          const compact = normalized.replace(/\s+/g, "");
          if (compact.indexOf("5小时") >= 0) {
            const slice = compact.slice(compact.indexOf("5小时"), compact.indexOf("5小时") + 240);
            const pm = slice.match(/(\d{1,3}(?:\.\d+)?)\s*%/);
            if (pm) fiveHour = Number(pm[1]) / 100;
          }
        }
        return { fiveHour, weekly };
      }
    },
    "kimi-code": {
      providerId: "kimi-code",
      providerName: "Kimi Code",
      extract(doc) {
        const token = localStorage.getItem("access_token");
        if (!token) return null;

        try {
          const request = new XMLHttpRequest();
          request.open("POST", "/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages", false);
          request.setRequestHeader("content-type", "application/json");
          request.setRequestHeader("authorization", "Bearer " + token);
          request.setRequestHeader("x-msh-platform", "web");
          request.setRequestHeader("x-msh-version", "1.0.0");
          request.setRequestHeader("x-language", "zh-CN");
          request.send(JSON.stringify({ scope: ["FEATURE_CODING"] }));

          if (request.status < 200 || request.status >= 300) return null;

          const payload = JSON.parse(request.responseText);
          const usages = Array.isArray(payload.usages) ? payload.usages : [];
          const usage = usages.find((item) => item && item.scope === "FEATURE_CODING") || usages[0];
          if (!usage) return { debug: { rawPayload: payload, reason: "no FEATURE_CODING usage" }, weekly: null, fiveHour: null };

          const clampRatio = (v) => Number.isFinite(v) ? Math.max(0, Math.min(1, v)) : -1;
          const pctText = (v) => v >= 0 ? Math.round(v * 100) + "%" : "";
          const toQuota = (d) => {
            if (!d) return null;
            const used = Number(d.used);
            const limit = Number(d.limit);
            if (!Number.isFinite(used) || !Number.isFinite(limit) || limit <= 0) return null;
            const remaining = limit - used;
            const ratio = clampRatio(remaining / limit);
            return { ratio, text: pctText(ratio), used, total: limit, resetAt: d.resetTime || "" };
          };

          const weekly = toQuota(usage.detail);
          const fiveHour = toQuota(
            Array.isArray(usage.limits) && usage.limits.length > 0 ? usage.limits[0].detail : null
          );

          const debug = {
            weeklyDetail: usage.detail,
            fiveHourDetail: Array.isArray(usage.limits) && usage.limits.length > 0 ? usage.limits[0].detail : null,
            weeklyResult: weekly,
            fiveHourResult: fiveHour,
          };

          if (!weekly && !fiveHour) return { debug, weekly: null, fiveHour: null };
          return {
            debug,
            weekly: weekly != null ? weekly.ratio : null,
            fiveHour: fiveHour != null ? fiveHour.ratio : null,
          };
        } catch {
          return null;
        }
      }
    },
    "glm-coding": {
      providerId: "glm-coding",
      providerName: "GLM Coding",
      extract(doc) {
        const cookiePrefix = "bigmodel_token_production=";
        const parts = document.cookie.split(";");
        let token = "";
        for (const part of parts) {
          const trimmed = part.trim();
          if (trimmed.indexOf(cookiePrefix) === 0) {
            token = decodeURIComponent(trimmed.slice(cookiePrefix.length));
            break;
          }
        }
        if (!token) return null;

        try {
          const request = new XMLHttpRequest();
          request.open("GET", "/api/monitor/usage/quota/limit", false);
          request.setRequestHeader("Authorization", token);
          request.send();

          if (request.status < 200 || request.status >= 300) return null;

          const payload = JSON.parse(request.responseText);
          const limits = payload?.data?.limits || [];
          const fiveHourLimit = limits.find((item) => item && item.type === "TOKENS_LIMIT" && item.unit === 3);
          const weeklyLimit = limits.find((item) => item && item.type === "TOKENS_LIMIT" && item.unit === 6);

          const toRatio = (limit) => {
            if (!limit) return null;
            const usedRatio = Number(limit.percentage) / 100;
            if (!Number.isFinite(usedRatio)) return null;
            return Math.max(0, Math.min(1, 1 - usedRatio));
          };

          const weekly = toRatio(weeklyLimit);
          const fiveHour = toRatio(fiveHourLimit);
          if (weekly == null && fiveHour == null) return null;
          return { weekly, fiveHour };
        } catch {
          return null;
        }
      }
    },
    "minimax": {
      providerId: "minimax",
      providerName: "MiniMax Coding",
      extract(doc) {
        const text = (doc.body ? doc.body.innerText : "") || "";
        const normalized = text.replace(/\u00a0/g, " ").replace(/％/g, "%").replace(/[ \t]+/g, " ").trim();
        if (normalized.indexOf("MiniMax") < 0 && !doc.location?.hostname?.includes("minimaxi.com")) {
          return null;
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
        if (fiveHour == null && weekly == null) return null;
        return { fiveHour, weekly };
      }
    }
  };

  const provider = PROVIDERS[providerId];
  if (!provider) return { success: false, error: "Unknown provider in tab" };

  const parsed = provider.extract(document);
  if (!parsed || (parsed.fiveHour == null && parsed.weekly == null)) {
    const text = (document.body ? document.body.innerText : "").toLowerCase();
    const isLogin = text.includes("log in") || text.includes("sign in") || text.includes("登录") || text.includes("login");
    if (isLogin) {
      return { success: false, error: "auth_error:未登录 " + provider.providerName + "，请先在浏览器中登录" };
    }
    return { success: false, needsRetry: true, error: "Could not find " + provider.providerName + " quota info on rendered page", debug: parsed?.debug };
  }

  return {
    success: true,
    data: {
      providerId: provider.providerId,
      providerName: provider.providerName,
      source: "browser_ext",
      status: "ok",
      weeklyRemainingRatio: parsed.weekly != null ? clamp(parsed.weekly) : -1,
      weeklyBalanceText: parsed.weekly != null ? pct(clamp(parsed.weekly)) : "",
      fiveHourRemainingRatio: parsed.fiveHour != null ? clamp(parsed.fiveHour) : -1,
      fiveHourBalanceText: parsed.fiveHour != null ? pct(clamp(parsed.fiveHour)) : "",
      remainingRatio: parsed.weekly != null ? clamp(parsed.weekly) : (parsed.fiveHour != null ? clamp(parsed.fiveHour) : -1),
      balanceText: parsed.weekly != null ? pct(clamp(parsed.weekly)) : (parsed.fiveHour != null ? pct(clamp(parsed.fiveHour)) : ""),
      updatedAt: new Date().toISOString(),
      _debug: parsed.debug || undefined,
    }
  };
}

function extractViaOffscreen(provider, timeout) {
  return new Promise((resolve, reject) => {
    let settled = false;
    let offscreenCreated = false;

    const timer = setTimeout(async () => {
      if (settled) return;
      settled = true;
      error("extractViaOffscreen: timeout for", provider.id);
      if (offscreenCreated) {
        try { await chrome.offscreen.closeDocument(); } catch {}
      }
      reject(new Error("Timeout extracting quota for " + provider.id));
    }, timeout);

    chrome.offscreen.createDocument(
      {
        url: "offscreen/offscreen.html",
        reasons: ["DOM_SCRAPING"],
        justification: "Fetch and parse quota pages to extract usage data",
      },
      () => {
        if (chrome.runtime.lastError) {
          if (settled) return;
          settled = true;
          clearTimeout(timer);
          error("extractViaOffscreen: createDocument failed:", chrome.runtime.lastError.message);
          reject(new Error(chrome.runtime.lastError.message));
          return;
        }

        offscreenCreated = true;

        chrome.runtime.sendMessage(
          {
            action: "extract",
            providerId: provider.id,
            quotaUrl: provider.quotaUrl,
          },
          async (response) => {
            if (settled) {
              if (offscreenCreated) {
                try { await chrome.offscreen.closeDocument(); } catch {}
              }
              return;
            }
            settled = true;
            clearTimeout(timer);

            if (offscreenCreated) {
              try { await chrome.offscreen.closeDocument(); } catch {}
            }

            if (chrome.runtime.lastError) {
              error("extractViaOffscreen: sendMessage failed:", chrome.runtime.lastError.message);
              reject(new Error(chrome.runtime.lastError.message));
              return;
            }

            if (response && response.success) {
              log("extractViaOffscreen: success for", provider.id);
              resolve(response.data);
            } else {
              const errMsg = (response && response.error) || "Extraction failed";
              error("extractViaOffscreen: failed for", provider.id, errMsg);
              reject(new Error(errMsg));
            }
          }
        );
      }
    );
  });
}

function handleOpenConsole(msg) {
  const { provider, url } = msg;
  log("handleOpenConsole:", provider, url);
  if (url) {
    chrome.tabs.create({ url });
  }
}

async function enqueueRefreshAll() {
  const enabled = await getEnabledPlans();
  if (enabled.length === 0) return;
  enqueueRefresh(enabled, null, 20000);
}

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  log("onMessage:", message);

  if (message.action === "manual-refresh") {
    const providers = message.providers;
    if (providers && Array.isArray(providers) && providers.length > 0) {
      enqueueRefresh(providers, null, 20000);
    } else {
      enqueueRefreshAll();
    }
    sendResponse({ success: true });
    return false;
  }

  if (message.action === "get-status") {
    const status = ws && ws.readyState === WebSocket.OPEN && authenticated
      ? "connected"
      : (ws && ws.readyState === WebSocket.CONNECTING ? "connecting" : "disconnected");
    sendResponse({ status });
    return false;
  }

  if (message.action === "auth-success-ack") {
    return false;
  }
});

chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "local" && changes[TOKEN_STORAGE_KEY]) {
    log("token changed, reconnecting");
    if (ws) {
      ws.close();
      ws = null;
    }
    authenticated = false;
    connect();
  }

  if (area === "local" && changes[PLANS_STORAGE_KEY]) {
    if (authenticated && ws && ws.readyState === WebSocket.OPEN) {
      log("plans changed, resending status");
      sendStatus().catch((err) => error("sendStatus failed", err));
    }
  }
});

chrome.alarms.onAlarm.addListener((alarm) => {
  if (alarm.name === ALARM_NAME) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      log("alarm: reconnecting");
      connect();
    }
  }

  if (alarm.name === AUTO_REFRESH_ALARM) {
    log("auto-refresh alarm: enqueueing all");
    enqueueRefreshAll();
  }
});

chrome.alarms.create(ALARM_NAME, { periodInMinutes: ALARM_INTERVAL_MINUTES });
chrome.alarms.create(AUTO_REFRESH_ALARM, { periodInMinutes: AUTO_REFRESH_INTERVAL_MINUTES });

log("service worker starting, initial connect");
connect();

enqueueRefreshAll();
