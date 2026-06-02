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
} from "./shared/ws-protocol.js";

const PROVIDERS = {
  codex: codexProvider,
  kimi_code: kimiCodeProvider,
  glm_coding: glmCodingProvider,
};

let ws = null;
let authenticated = false;
let heartbeatTimer = null;

function sendJson(msg) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(msg));
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
    return;
  }

  const token = await getToken();
  if (!token) {
    console.warn("[dde-coding-plan] no token configured, skipping connection");
    return;
  }

  try {
    ws = new WebSocket(WS_URL);
  } catch (err) {
    console.error("[dde-coding-plan] WebSocket connect failed:", err);
    return;
  }

  ws.onopen = () => {
    console.log("[dde-coding-plan] WebSocket connected");
    sendJson({ type: MSG_TYPE_AUTH, token });
  };

  ws.onmessage = (event) => {
    let msg;
    try {
      msg = JSON.parse(event.data);
    } catch {
      console.warn("[dde-coding-plan] invalid JSON:", event.data);
      return;
    }

    handleMessage(msg);
  };

  ws.onclose = () => {
    console.log("[dde-coding-plan] WebSocket closed");
    stopHeartbeat();
    authenticated = false;
    ws = null;
  };

  ws.onerror = (err) => {
    console.error("[dde-coding-plan] WebSocket error:", err);
  };

  ws.onpong = () => {};
}

function handleMessage(msg) {
  const { type } = msg;

  if (type === MSG_TYPE_REFRESH_REQUEST) {
    handleRefreshRequest(msg);
  } else if (type === MSG_TYPE_OPEN_CONSOLE) {
    handleOpenConsole(msg);
  }
}

async function handleRefreshRequest(msg) {
  const { requestId, providers: providerIds, timeout } = msg;
  console.log("[dde-coding-plan] refresh request:", requestId, providerIds);

  for (const providerId of providerIds) {
    const provider = PROVIDERS[providerId];
    if (!provider) {
      sendJson({
        type: MSG_TYPE_REFRESH_RESULT,
        requestId,
        provider: providerId,
        data: {
          providerId,
          providerName: providerId,
          source: "browser_ext",
          status: "parse_error",
          message: `Unknown provider: ${providerId}`,
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
      console.error("[dde-coding-plan] extraction error:", providerId, err);
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
      reject(new Error(`Timeout extracting quota for ${provider.id}`));
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
              reject(new Error(chrome.runtime.lastError.message));
              return;
            }

            if (response && response.success) {
              resolve(response.data);
            } else {
              reject(new Error((response && response.error) || "Extraction failed"));
            }
          }
        );
      }
    );
  });
}

function handleOpenConsole(msg) {
  const { provider, url } = msg;
  if (url) {
    chrome.tabs.create({ url });
  }
}

chrome.storage.onChanged.addListener((changes, area) => {
  if (area === "local" && changes[TOKEN_STORAGE_KEY]) {
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
      connect();
    }
  }
});

chrome.alarms.create(ALARM_NAME, { periodInMinutes: ALARM_INTERVAL_MINUTES });

connect();
