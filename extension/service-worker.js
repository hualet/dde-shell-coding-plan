import { codexProvider } from "../providers/codex.js";
import { kimiCodeProvider } from "../providers/kimi-code.js";
import { glmCodingProvider } from "../providers/glm-coding.js";

import {
  MSG_TYPE_AUTH,
  MSG_TYPE_STATUS,
  MSG_TYPE_REFRESH_REQUEST,
  MSG_TYPE_REFRESH_RESULT,
  MSG_TYPE_REFRESH_PROGRESS,
  MSG_TYPE_OPEN_CONSOLE,
  WS_URL,
  HEARTBEAT_INTERVAL_MS,
  ALARM_NAME,
  ALARM_INTERVAL_MINUTES,
  TOKEN_STORAGE_KEY,
  STATUS_STORAGE_KEY,
} from "./shared/ws-protocol.js";

const PROVIDERS = {
  codex: codexProvider,
  "kimi-code": kimiCodeProvider,
  "glm-coding": glmCodingProvider,
};

let ws = null;
let authenticated = false;
let heartbeatTimer = null;
let connectTimeoutTimer = null;

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
    warn("cannot send, ws not open, readyState:", ws ? ws.readyState : "null");
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
      ws.ping();
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
    log("connect: already connected or connecting, readyState:", ws.readyState);
    return;
  }

  const token = await getToken();
  if (!token) {
    warn("connect: no token configured, skipping connection");
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

  ws.onclose = (event) => {
    log("connect: WebSocket onclose, code:", event.code, "reason:", event.reason);
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

  ws.onpong = () => {
    log("onpong received");
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

async function handleRefreshRequest(msg) {
  const { requestId, providers: providerIds, timeout } = msg;
  log("handleRefreshRequest:", requestId, providerIds, "timeout:", timeout);

  if (!authenticated) {
    warn("handleRefreshRequest: not authenticated, ignoring");
    return;
  }

  for (const providerId of providerIds) {
    const provider = PROVIDERS[providerId];
    if (!provider) {
      warn("handleRefreshRequest: unknown provider:", providerId, "available:", Object.keys(PROVIDERS));
      sendJson({
        type: MSG_TYPE_REFRESH_RESULT,
        requestId,
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
      continue;
    }

    try {
      sendJson({
        type: MSG_TYPE_REFRESH_PROGRESS,
        requestId,
        provider: providerId,
        status: "loading",
        message: "正在加载额度页面...",
      });

      const result = await extractProviderQuota(provider, timeout || 15000);

      sendJson({
        type: MSG_TYPE_REFRESH_RESULT,
        requestId,
        provider: providerId,
        data: {
          providerId,
          providerName: provider.name,
          source: "browser_ext",
          status: result.status,
          remainingRatio: result.remainingRatio ?? -1,
          fiveHourRemainingRatio: result.fiveHourRemainingRatio ?? -1,
          balanceText: result.balanceText || "",
          fiveHourBalanceText: result.fiveHourBalanceText || "",
          used: result.used ?? -1,
          total: result.total ?? -1,
          unit: result.unit || "credit",
          updatedAt: new Date().toISOString(),
          consoleUrl: provider.consoleUrl || "",
          message: result.message || "",
        },
      });
    } catch (err) {
      error("handleRefreshRequest: extraction error for", providerId, err);
      sendJson({
        type: MSG_TYPE_REFRESH_RESULT,
        requestId,
        provider: providerId,
        data: {
          providerId,
          providerName: provider.name,
          source: "browser_ext",
          status: "network_error",
          message: err.message || "Unknown error",
          updatedAt: new Date().toISOString(),
        },
      });
    }
  }
}

function extractProviderQuota(provider, timeout) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error("Timeout extracting quota for " + provider.id));
    }, timeout);

    chrome.offscreen.createDocument(
      {
        url: "offscreen/offscreen.html",
        reasons: ["IFRAME_SCRIPTING"],
        justification: "Load quota pages to extract usage data",
      },
      () => {
        if (chrome.runtime.lastError) {
          clearTimeout(timer);
          error("extractProviderQuota: createDocument failed:", chrome.runtime.lastError.message);
          chrome.offscreen.closeDocument();
          reject(new Error(chrome.runtime.lastError.message));
          return;
        }

        chrome.runtime.sendMessage(
          {
            action: "extract",
            providerId: provider.id,
            quotaUrl: provider.quotaUrl,
          },
          (response) => {
            clearTimeout(timer);
            chrome.offscreen.closeDocument();

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

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  log("onMessage:", message);

  if (message.action === "manual-refresh") {
    if (ws && ws.readyState === WebSocket.OPEN && authenticated) {
      sendStatus();
      sendResponse({ success: true });
    } else {
      connect();
      sendResponse({ success: false, reason: "not connected" });
    }
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
    log("popup acknowledged auth success");
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
});

chrome.alarms.create(ALARM_NAME, { periodInMinutes: ALARM_INTERVAL_MINUTES });

log("service worker starting, initial connect");
connect();
