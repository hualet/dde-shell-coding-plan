import {
  TOKEN_STORAGE_KEY,
  STATUS_STORAGE_KEY,
} from "../shared/ws-protocol.js";

const tokenInput = document.getElementById("tokenInput");
const saveBtn = document.getElementById("saveBtn");
const refreshBtn = document.getElementById("refreshBtn");
const statusDot = document.getElementById("statusDot");
const statusText = document.getElementById("statusText");
const lastRefresh = document.getElementById("lastRefresh");

const STATUS_LABELS = {
  disconnected: "未连接",
  connecting: "正在连接...",
  authenticating: "正在认证...",
  connected: "已连接",
  auth_failed: "认证失败（Token 不匹配）",
  error: "连接失败",
  timeout: "连接超时",
  no_token: "请输入 Token",
};

function updateStatus(status) {
  const label = STATUS_LABELS[status] || status;
  statusText.textContent = label;

  if (status === "connected") {
    statusDot.className = "status-dot connected";
  } else {
    statusDot.className = "status-dot disconnected";
  }
}

async function init() {
  const result = await chrome.storage.local.get(TOKEN_STORAGE_KEY);
  if (result[TOKEN_STORAGE_KEY]) {
    tokenInput.value = result[TOKEN_STORAGE_KEY];
  }

  const statusResult = await chrome.storage.local.get(STATUS_STORAGE_KEY);
  updateStatus(statusResult[STATUS_STORAGE_KEY] || "disconnected");

  chrome.storage.onChanged.addListener((changes, area) => {
    if (area === "local" && changes[STATUS_STORAGE_KEY]) {
      updateStatus(changes[STATUS_STORAGE_KEY].newValue);
    }
  });
}

saveBtn.addEventListener("click", async () => {
  const token = tokenInput.value.trim();
  if (!token) {
    updateStatus("no_token");
    return;
  }
  await chrome.storage.local.set({ [TOKEN_STORAGE_KEY]: token });
  updateStatus("connecting");

  let timeout = setTimeout(() => {
    updateStatus("timeout");
  }, 15000);

  chrome.storage.onChanged.addListener(function listener(changes, area) {
    if (area === "local" && changes[STATUS_STORAGE_KEY]) {
      const newStatus = changes[STATUS_STORAGE_KEY].newValue;
      if (newStatus === "connected" || newStatus === "auth_failed" || newStatus === "error" || newStatus === "timeout") {
        clearTimeout(timeout);
        chrome.storage.onChanged.removeListener(listener);
      }
    }
  });
});

refreshBtn.addEventListener("click", async () => {
  statusText.textContent = "请求刷新中...";
  try {
    const response = await chrome.runtime.sendMessage({ action: "get-status" });
    if (response && response.status === "connected") {
      lastRefresh.textContent = "已发送刷新请求 " + new Date().toLocaleTimeString();
    } else {
      lastRefresh.textContent = "扩展未连接，请先配对";
    }
  } catch {
    lastRefresh.textContent = "Service worker 未就绪";
  }
});

init();
