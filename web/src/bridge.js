const isQWebChannel = () => !!(window.qt && window.qt.webChannelTransport);

let bridgePromise = null;

function getBridge() {
  if (bridgePromise) return bridgePromise;
  if (isQWebChannel()) {
    if (typeof window.QWebChannel !== 'function') {
      bridgePromise = Promise.resolve(null);
      return bridgePromise;
    }

    bridgePromise = new Promise((resolve) => {
      // eslint-disable-next-line no-undef
      new window.QWebChannel(window.qt.webChannelTransport, (channel) => {
        resolve(channel.objects.bridge);
      });
    }).catch(() => null);
  } else {
    bridgePromise = Promise.resolve(null);
  }
  return bridgePromise;
}

const MOCK_PROVIDERS = [
  { id: 'codex', name: 'Codex / ChatGPT', source: 'webview', loginUrl: 'https://chatgpt.com/auth/login', quotaUrl: 'https://chatgpt.com/#settings/usage', consoleUrl: 'https://chatgpt.com/#settings/usage' },
  { id: 'kimi-code', name: 'Kimi Code', source: 'webview', loginUrl: 'https://www.kimi.com/login', quotaUrl: 'https://www.kimi.com/', consoleUrl: 'https://www.kimi.com/' },
  { id: 'glm-coding', name: 'GLM Coding', source: 'webview', loginUrl: 'https://chatglm.cn/login', quotaUrl: 'https://chatglm.cn/', consoleUrl: 'https://open.bigmodel.cn/usercenter/overview' },
];

const MOCK_SNAPSHOTS = [
  { providerId: 'codex', providerName: 'Codex / ChatGPT', source: 'webview', status: 'ok', severity: 'normal', remainingRatio: 0.75, used: -1, total: -1, unit: '', balanceText: '75%', message: '', updatedAt: new Date().toISOString(), consoleUrl: 'https://chatgpt.com/#settings/usage' },
  { providerId: 'kimi-code', providerName: 'Kimi Code', source: 'webview', status: 'auth_error', severity: 'error', remainingRatio: -1, used: -1, total: -1, unit: '', balanceText: '', message: '等待登录', updatedAt: new Date().toISOString(), consoleUrl: 'https://www.kimi.com/' },
  { providerId: 'glm-coding', providerName: 'GLM Coding', source: 'webview', status: 'ok', severity: 'warning', remainingRatio: 0.22, used: -1, total: -1, unit: '', balanceText: '22%', message: '', updatedAt: new Date().toISOString(), consoleUrl: 'https://open.bigmodel.cn/usercenter/overview' },
];

async function callBridge(method, ...args) {
  const b = await getBridge();
  if (b && typeof b[method] === 'function') {
    return new Promise((resolve) => {
      b[method](...args, (result) => resolve(result));
    });
  }
  return null;
}

export async function fetchProviders() {
  const b = await getBridge();
  if (b) {
    return new Promise((resolve) => {
      let attempts = 0;
      const check = () => {
        const p = b.providers;
        if (p && p.length > 0) { resolve(p); return; }
        attempts += 1;
        if (attempts >= 30) { resolve([]); return; }
        setTimeout(check, 100);
      };
      check();
    });
  }
  return MOCK_PROVIDERS;
}

export async function fetchSnapshots() {
  const b = await getBridge();
  if (b) {
    return new Promise((resolve) => {
      let attempts = 0;
      const check = () => {
        const s = b.snapshots;
        if (s && s.length > 0) { resolve(s); return; }
        attempts += 1;
        if (attempts >= 30) { resolve([]); return; }
        setTimeout(check, 100);
      };
      check();
    });
  }
  return MOCK_SNAPSHOTS;
}

export async function refreshAll() {
  return callBridge('refreshAll');
}

export async function refreshProvider(providerId) {
  return callBridge('refreshProvider', providerId);
}

export async function openConsole(providerId) {
  return callBridge('openConsole', providerId);
}

export async function requestLogin(providerId) {
  return callBridge('requestLogin', providerId);
}

export async function finishLogin(providerId) {
  return callBridge('finishLogin', providerId);
}

export async function setManualRatio(providerId, ratio) {
  return callBridge('setManualRatio', providerId, ratio);
}

export function isLoggedIn(snapshot) {
  if (!snapshot) return false;
  return snapshot.status === 'ok' || snapshot.status === 'warning' || snapshot.status === 'exhausted';
}

export { isQWebChannel, MOCK_PROVIDERS, MOCK_SNAPSHOTS };
