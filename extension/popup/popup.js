import {
  TOKEN_STORAGE_KEY,
} from "../shared/ws-protocol.js";

const tokenInput = document.getElementById("tokenInput");
const saveBtn = document.getElementById("saveBtn");
const refreshBtn = document.getElementById("refreshBtn");
const statusDot = document.getElementById("statusDot");
const statusText = document.getElementById("statusText");
const lastRefresh = document.getElementById("lastRefresh");

async function init() {
  const result = await chrome.storage.local.get(TOKEN_STORAGE_KEY);
  if (result[TOKEN_STORAGE_KEY]) {
    tokenInput.value = result[TOKEN_STORAGE_KEY];
  }

  const statusResult = await chrome.storage.local.get("ws-status");
  updateStatus(statusResult["ws-status"] || "disconnected");
}

function updateStatus(status) {
  if (status === "connected") {
    statusDot.className = "status-dot connected";
    statusText.textContent = "已连接";
  } else {
    statusDot.className = "status-dot disconnected";
    statusText.textContent = "未连接";
  }
}

saveBtn.addEventListener("click", async () => {
  const token = tokenInput.value.trim();
  if (!token) return;
  await chrome.storage.local.set({ [TOKEN_STORAGE_KEY]: token });
  statusText.textContent = "正在连接...";
});

refreshBtn.addEventListener("click", async () => {
  statusText.textContent = "请求刷新中...";
  try {
    await chrome.runtime.sendMessage({ action: "manual-refresh" });
  } catch {
    // service worker may not be ready
  }
});

init();
