export { codexProvider } from "./codex.js";
export { kimiCodeProvider, detailToQuota } from "./kimi-code.js";
export { glmCodingProvider } from "./glm-coding.js";
export { minimaxProvider } from "./minimax.js";

import { codexProvider } from "./codex.js";
import { kimiCodeProvider } from "./kimi-code.js";
import { glmCodingProvider } from "./glm-coding.js";
import { minimaxProvider } from "./minimax.js";

export const ALL_PROVIDERS = [
  codexProvider,
  kimiCodeProvider,
  glmCodingProvider,
  minimaxProvider,
];

export const PROVIDER_DEFAULTS = ALL_PROVIDERS.map((p) => ({
  id: p.id,
  name: p.name,
  quotaUrl: p.quotaUrl,
  consoleUrl: p.consoleUrl,
  loginUrl: p.loginUrl,
  allowedOrigin: p.allowedOrigin,
}));

export function buildProviderMap() {
  const map = {};
  for (const p of ALL_PROVIDERS) {
    map[p.id] = p;
  }
  return map;
}

export function getTabProviderIds() {
  return new Set(
    ALL_PROVIDERS
      .filter((p) => p.extractionMode === "tab")
      .map((p) => p.id)
  );
}
