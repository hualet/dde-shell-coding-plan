import { codexProvider } from "./providers/codex.js";
import { kimiCodeProvider } from "./providers/kimi-code.js";
import { glmCodingProvider } from "./providers/glm-coding.js";

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
} from "./shared/storage.js";

const PROVIDERS = {
  codex: codexProvider,
  "kimi-code": kimiCodeProvider,
  "glm-coding": glmCodingProvider,
};

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

function sendStatus() {
  const availableProviders = Object.keys(PROVIDERS);
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
    handleRefreshRequest(msg);
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
    sendStatus();
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

      if (wsRequestId) {
        sendJson({
          type: MSG_TYPE_REFRESH_RESULT,
          requestId: wsRequestId,
          provider: providerId,
          data,
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

  enqueueRefresh(providerIds, requestId, timeout || 15000);
}

function extractProviderQuota(provider, timeout) {
  return new Promise((resolve, reject) => {
    let settled = false;
    let offscreenCreated = false;

    const timer = setTimeout(async () => {
      if (settled) return;
      settled = true;
      error("extractProviderQuota: timeout for", provider.id);
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
          error("extractProviderQuota: createDocument failed:", chrome.runtime.lastError.message);
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
              error("extractProviderQuota: sendMessage failed:", chrome.runtime.lastError.message);
              reject(new Error(chrome.runtime.lastError.message));
              return;
            }

            if (response && response.success) {
              log("extractProviderQuota: success for", provider.id);
              resolve(response.data);
            } else {
              const errMsg = (response && response.error) || "Extraction failed";
              error("extractProviderQuota: failed for", provider.id, errMsg);
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
