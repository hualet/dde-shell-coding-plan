import { codexProvider } from "../providers/codex.js";
import { kimiCodeProvider } from "../providers/kimi-code.js";
import { glmCodingProvider } from "../providers/glm-coding.js";

const PROVIDERS = {
  codex: codexProvider,
  "kimi-code": kimiCodeProvider,
  "glm-coding": glmCodingProvider,
};

const FETCH_TIMEOUT = 15000;

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (msg.action !== "extract") return false;

  const { providerId, quotaUrl } = msg;
  const provider = PROVIDERS[providerId];

  if (!provider) {
    sendResponse({ success: false, error: `Unknown provider: ${providerId}` });
    return false;
  }

  extractViaFetch(provider, quotaUrl)
    .then((data) => sendResponse({ success: true, data }))
    .catch((err) => sendResponse({ success: false, error: err.message }));

  return true;
});

async function extractViaFetch(provider, quotaUrl) {
  const html = await fetchWithTimeout(quotaUrl);
  const parser = new DOMParser();
  const doc = parser.parseFromString(html, "text/html");

  checkLoginRedirect(provider, doc, quotaUrl);

  if (provider.extractViaApi) {
    const apiResult = await provider.extractViaApi(html, doc);
    if (apiResult) {
      return provider.normalizeSnapshot(apiResult);
    }
  }

  const extraction = provider.extractQuota(doc);
  if (extraction && extraction.success) {
    return provider.normalizeSnapshot(extraction.raw);
  }

  const textResult = extractFromRawText(provider, html);
  if (textResult) {
    return provider.normalizeSnapshot(textResult);
  }

  const reason = extraction?.reason || "Could not parse quota data";
  const isLogin = detectLoginState(provider, html);
  if (isLogin) {
    return provider.normalizeSnapshot({
      status: "auth_error",
      message: "登录已过期或未登录，请在浏览器中访问 " + provider.loginUrl + " 完成登录",
    });
  }

  return provider.normalizeSnapshot({
    status: "parse_error",
    message: reason,
  });
}

async function fetchWithTimeout(url) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), FETCH_TIMEOUT);

  try {
    const response = await fetch(url, {
      credentials: "include",
      redirect: "follow",
      signal: controller.signal,
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status} for ${url}`);
    }

    return await response.text();
  } finally {
    clearTimeout(timer);
  }
}

function checkLoginRedirect(provider, doc, quotaUrl) {
  const title = (doc.title || "").toLowerCase();
  const bodyText = (doc.body ? doc.body.innerText : "").toLowerCase();

  if (title.includes("login") || title.includes("sign in") || title.includes("登录")) {
    throw new Error("auth_error:页面重定向到登录页");
  }

  if (bodyText.includes("log in to continue") || bodyText.includes("sign in to continue")) {
    throw new Error("auth_error:需要登录才能访问");
  }
}

function detectLoginState(provider, html) {
  const lower = html.toLowerCase();
  const loginIndicators = [
    "login",
    "sign in",
    "sign-in",
    "auth/login",
    "authentication",
    "登录",
  ];

  for (const indicator of loginIndicators) {
    if (lower.includes(indicator)) return true;
  }

  return false;
}

function extractFromRawText(provider, html) {
  const text = html.replace(/<[^>]+>/g, " ").replace(/\s+/g, " ");
  const lines = text.split(/[\n.]+/).map((l) => l.trim()).filter((l) => l.length > 0);

  const parser = new DOMParser();
  const doc = parser.parseFromString(html, "text/html");
  const extraction = provider.extractQuota(doc);
  if (extraction && extraction.success) {
    return extraction.raw;
  }

  return null;
}
