import { codexProvider } from "../providers/codex.js";
import { kimiCodeProvider } from "../providers/kimi-code.js";
import { glmCodingProvider } from "../providers/glm-coding.js";

const PROVIDERS = {
  codex: codexProvider,
  "kimi-code": kimiCodeProvider,
  "glm-coding": glmCodingProvider,
};

const IFRAME_LOAD_TIMEOUT = 10000;
const POLL_INTERVAL = 500;
const MAX_POLL_ATTEMPTS = 20;

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (msg.action !== "extract") return false;

  const { providerId, quotaUrl } = msg;
  const provider = PROVIDERS[providerId];

  if (!provider) {
    sendResponse({ success: false, error: `Unknown provider: ${providerId}` });
    return false;
  }

  extractFromIframe(provider, quotaUrl)
    .then((data) => sendResponse({ success: true, data }))
    .catch((err) => sendResponse({ success: false, error: err.message }));

  return true;
});

function extractFromIframe(provider, quotaUrl) {
  return new Promise((resolve, reject) => {
    const iframe = document.createElement("iframe");
    iframe.style.width = "1024px";
    iframe.style.height = "768px";
    iframe.style.position = "absolute";
    iframe.style.left = "-9999px";

    const timeout = setTimeout(() => {
      cleanup();
      reject(new Error(`Timeout loading ${quotaUrl}`));
    }, IFRAME_LOAD_TIMEOUT);

    let pollAttempts = 0;

    iframe.onload = () => {
      pollForContent();
    };

    iframe.onerror = () => {
      cleanup();
      reject(new Error(`Failed to load ${quotaUrl}`));
    };

    function pollForContent() {
      pollAttempts++;
      try {
        const doc = iframe.contentDocument;
        if (!doc || !doc.body || !doc.body.innerHTML) {
          if (pollAttempts < MAX_POLL_ATTEMPTS) {
            setTimeout(pollForContent, POLL_INTERVAL);
            return;
          }
          cleanup();
          reject(new Error("Page content never appeared"));
          return;
        }

        const extraction = provider.extractQuota(doc);
        if (!extraction || !extraction.success) {
          if (pollAttempts < MAX_POLL_ATTEMPTS) {
            setTimeout(pollForContent, POLL_INTERVAL);
            return;
          }
          const result = {
            status: "parse_error",
            message: extraction?.reason || "Could not parse quota data",
            updatedAt: new Date().toISOString(),
          };
          cleanup();
          resolve(provider.normalizeSnapshot(result));
          return;
        }

        const normalized = provider.normalizeSnapshot(extraction.raw);
        cleanup();
        resolve(normalized);
      } catch (err) {
        if (pollAttempts < MAX_POLL_ATTEMPTS) {
          setTimeout(pollForContent, POLL_INTERVAL);
          return;
        }
        cleanup();
        reject(err);
      }
    }

    function cleanup() {
      clearTimeout(timeout);
      if (iframe.parentNode) {
        iframe.parentNode.removeChild(iframe);
      }
    }

    document.body.appendChild(iframe);
    iframe.src = quotaUrl;
  });
}
