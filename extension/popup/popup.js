import { STATUS_STORAGE_KEY } from "../shared/ws-protocol.js";
import { STATUS_LABELS } from "../shared/ui-labels.js";
import {
  DEFAULT_PROVIDERS,
  getEnabledPlans,
  getQuotaCache,
  QUOTA_CACHE_KEY,
  PLANS_STORAGE_KEY,
} from "../shared/storage.js";

const statusDot = document.getElementById("statusDot");
const statusLabel = document.getElementById("statusLabel");
const quotaList = document.getElementById("quotaList");
const refreshBtn = document.getElementById("refreshBtn");
const settingsBtn = document.getElementById("settingsBtn");

function updateConnectionStatus(status) {
  statusLabel.textContent = STATUS_LABELS[status] || status;
  statusDot.className = "status-dot " + (status || "disconnected");
}

function getSeverityClass(ratio) {
  if (ratio < 0) return "unknown";
  if (ratio < 0.10) return "critical";
  if (ratio <= 0.30) return "warn";
  return "ok";
}

function formatRatio(ratio) {
  if (ratio < 0) return "不可用";
  return Math.round(ratio * 100) + "%";
}

function buildQuotaRow(label, ratio, severity) {
  const pct = ratio >= 0 ? Math.round(ratio * 100) : 0;
  return `
    <div class="quota-row">
      <span class="quota-row-label">${label}</span>
      <div class="quota-row-bar">
        <div class="quota-bar-fill bar-${severity}" style="width:${pct}%"></div>
      </div>
      <span class="quota-row-value ratio-${severity}">${formatRatio(ratio)}</span>
    </div>`;
}

function renderQuotaList(enabled, cache) {
  if (enabled.length === 0) {
    quotaList.innerHTML = `
      <div class="empty-state">
        <p>尚未添加任何 Coding Plan</p>
        <button class="btn-primary" id="openSettings">前往配置</button>
      </div>`;
    document.getElementById("openSettings")?.addEventListener("click", () => {
      chrome.runtime.openOptionsPage();
    });
    return;
  }

  quotaList.innerHTML = "";
  for (const planId of enabled) {
    const provider = DEFAULT_PROVIDERS.find((p) => p.id === planId);
    if (!provider) continue;

    const cached = cache[planId];
    const card = document.createElement("div");

    if (!cached || (cached.status !== "ok" && cached.status !== "parse_error")) {
      const message = cached?.message || "等待刷新";
      const stateClass = cached ? "error-state" : "unknown-state";
      card.className = `quota-card ${stateClass}`;
      card.innerHTML = `
        <div class="quota-header">
          <span class="quota-name">${provider.name}</span>
          <span class="quota-ratio ratio-unknown">--</span>
        </div>
        <div class="quota-meta">
          <span class="error-msg">${message}</span>
          ${cached?.cachedAt ? '<span>' + new Date(cached.cachedAt).toLocaleTimeString() + '</span>' : ''}
        </div>`;
      quotaList.appendChild(card);
      continue;
    }

    const weeklyRatio = cached.weeklyRemainingRatio ?? cached.remainingRatio ?? -1;
    const fiveHourRatio = cached.fiveHourRemainingRatio ?? -1;

    const weeklySeverity = getSeverityClass(weeklyRatio);
    const fiveHourSeverity = getSeverityClass(fiveHourRatio);

    const mainRatio = weeklyRatio >= 0 ? weeklyRatio : fiveHourRatio;
    const mainSeverity = getSeverityClass(mainRatio);

    card.className = `quota-card ${mainSeverity}-state`;
    card.innerHTML = `
      <div class="quota-header">
        <span class="quota-name">${provider.name}</span>
        <span class="quota-ratio ratio-${mainSeverity}">${formatRatio(mainRatio)}</span>
      </div>
      ${buildQuotaRow("周额度", weeklyRatio, weeklySeverity)}
      ${buildQuotaRow("5h 额度", fiveHourRatio, fiveHourSeverity)}
      <div class="quota-meta">
        <span>${cached.message || ""}</span>
        <span>${cached.cachedAt ? new Date(cached.cachedAt).toLocaleTimeString() : ""}</span>
      </div>`;

    quotaList.appendChild(card);
  }
}

async function refreshDisplay() {
  const enabled = await getEnabledPlans();
  const cache = await getQuotaCache();
  renderQuotaList(enabled, cache);
}

async function init() {
  const statusResult = await chrome.storage.local.get(STATUS_STORAGE_KEY);
  updateConnectionStatus(statusResult[STATUS_STORAGE_KEY] || "disconnected");

  chrome.storage.onChanged.addListener((changes, area) => {
    if (area === "local" && changes[STATUS_STORAGE_KEY]) {
      updateConnectionStatus(changes[STATUS_STORAGE_KEY].newValue);
    }
    if (area === "local" && changes[QUOTA_CACHE_KEY]) {
      refreshDisplay();
    }
    if (area === "local" && changes[PLANS_STORAGE_KEY]) {
      refreshDisplay();
    }
  });

  refreshBtn.addEventListener("click", () => {
    refreshBtn.innerHTML = '<span class="refreshing"></span>';
    chrome.runtime.sendMessage({ action: "manual-refresh", providers: null }, () => {
      setTimeout(() => {
        refreshBtn.innerHTML = "&#x21bb;";
      }, 1500);
    });
  });

  settingsBtn.addEventListener("click", () => {
    chrome.runtime.openOptionsPage();
  });

  await refreshDisplay();
}

init();
