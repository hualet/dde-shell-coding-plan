import {
  TOKEN_STORAGE_KEY,
  STATUS_STORAGE_KEY,
} from "../shared/ws-protocol.js";
import {
  DEFAULT_PROVIDERS,
  getEnabledPlans,
  addPlan,
  removePlan,
  getQuotaCache,
  QUOTA_CACHE_KEY,
} from "../shared/storage.js";

const statusDot = document.getElementById("statusDot");
const statusText = document.getElementById("statusText");
const tokenInput = document.getElementById("tokenInput");
const saveTokenBtn = document.getElementById("saveTokenBtn");
const testConnBtn = document.getElementById("testConnBtn");
const refreshAllBtn = document.getElementById("refreshAllBtn");
const planList = document.getElementById("planList");
const addPlanList = document.getElementById("addPlanList");
const toastEl = document.getElementById("toast");

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

let toastTimer = null;

function showToast(msg) {
  toastEl.textContent = msg;
  toastEl.classList.add("show");
  if (toastTimer) clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toastEl.classList.remove("show"), 2500);
}

function updateStatus(status) {
  const label = STATUS_LABELS[status] || status;
  statusText.textContent = label;
  statusDot.className = "status-dot " + (status || "disconnected");
}

function switchPanel(name) {
  document.querySelectorAll(".panel").forEach((p) => p.classList.remove("active"));
  document.querySelectorAll(".sidebar nav a").forEach((a) => a.classList.remove("active"));
  const panel = document.getElementById("panel-" + name);
  if (panel) panel.classList.add("active");
  const link = document.querySelector(`.sidebar nav a[data-panel="${name}"]`);
  if (link) link.classList.add("active");
}

async function renderPlans() {
  const enabled = await getEnabledPlans();
  const cache = await getQuotaCache();

  planList.innerHTML = "";
  for (const planId of enabled) {
    const provider = DEFAULT_PROVIDERS.find((p) => p.id === planId);
    if (!provider) continue;

    const cached = cache[planId];
    let statusLabel = "等待刷新";
    let ratioText = "";
    if (cached) {
      if (cached.status === "ok") {
        ratioText = cached.remainingRatio >= 0
          ? Math.round(cached.remainingRatio * 100) + "%"
          : cached.balanceText || "";
        statusLabel = ratioText;
      } else {
        statusLabel = cached.message || cached.status || "异常";
      }
    }

    const card = document.createElement("div");
    card.className = "plan-card enabled";
    card.innerHTML = `
      <div class="plan-info">
        <div class="plan-name">${provider.name}</div>
        <div class="plan-id">${statusLabel}${cached && cached.cachedAt ? " · " + new Date(cached.cachedAt).toLocaleTimeString() : ""}</div>
      </div>
      <div class="plan-actions">
        <button class="btn-secondary btn-refresh-plan" data-id="${planId}" style="font-size:12px;padding:5px 12px;">刷新</button>
        <button class="btn-danger btn-remove-plan" data-id="${planId}" style="font-size:12px;padding:5px 12px;">移除</button>
      </div>
    `;
    planList.appendChild(card);
  }

  planList.querySelectorAll(".btn-remove-plan").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const id = btn.dataset.id;
      await removePlan(id);
      await renderPlans();
      showToast("已移除 " + id);
    });
  });

  planList.querySelectorAll(".btn-refresh-plan").forEach((btn) => {
    btn.addEventListener("click", () => {
      chrome.runtime.sendMessage({ action: "manual-refresh", providers: [btn.dataset.id] });
      showToast("已请求刷新 " + btn.dataset.id);
    });
  });

  const disabled = DEFAULT_PROVIDERS.filter((p) => !enabled.includes(p.id));
  addPlanList.innerHTML = "";
  if (disabled.length === 0) {
    addPlanList.innerHTML = '<div style="color:#9ca3af;font-size:13px;">所有 Plan 已启用</div>';
    return;
  }
  for (const provider of disabled) {
    const card = document.createElement("div");
    card.className = "plan-card";
    card.innerHTML = `
      <div class="plan-info">
        <div class="plan-name">${provider.name}</div>
        <div class="plan-id">${provider.id}</div>
      </div>
      <div class="plan-actions">
        <button class="btn-primary btn-add-plan" data-id="${provider.id}" style="font-size:12px;padding:5px 12px;">添加</button>
      </div>
    `;
    addPlanList.appendChild(card);
  }

  addPlanList.querySelectorAll(".btn-add-plan").forEach((btn) => {
    btn.addEventListener("click", async () => {
      await addPlan(btn.dataset.id);
      await renderPlans();
      showToast("已添加 " + btn.dataset.id);
    });
  });
}

async function init() {
  document.querySelectorAll(".sidebar nav a").forEach((a) => {
    a.addEventListener("click", () => switchPanel(a.dataset.panel));
  });

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
    if (area === "local" && changes[QUOTA_CACHE_KEY]) {
      renderPlans();
    }
  });

  saveTokenBtn.addEventListener("click", async () => {
    const token = tokenInput.value.trim();
    if (!token) {
      updateStatus("no_token");
      return;
    }
    await chrome.storage.local.set({ [TOKEN_STORAGE_KEY]: token });
    updateStatus("connecting");
    saveTokenBtn.disabled = true;
    setTimeout(() => { saveTokenBtn.disabled = false; }, 5000);
    showToast("Token 已保存，正在连接...");
  });

  testConnBtn.addEventListener("click", async () => {
    try {
      const response = await chrome.runtime.sendMessage({ action: "get-status" });
      if (response && response.status === "connected") {
        showToast("连接正常");
      } else {
        showToast("未连接: " + (response ? response.status : "无响应"));
      }
    } catch {
      showToast("Service worker 未就绪");
    }
  });

  refreshAllBtn.addEventListener("click", () => {
    chrome.runtime.sendMessage({ action: "manual-refresh", providers: null });
    showToast("已请求刷新全部 Plan");
  });

  await renderPlans();
}

init();
