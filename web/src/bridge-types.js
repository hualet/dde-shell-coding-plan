/**
 * TypeScript type definitions for the Coding Plan QWebChannel Bridge.
 *
 * These types describe the contract between the Native (C++ QWebChannel)
 * and the Web (React) layers. The actual runtime is plain JS — these
 * definitions serve as documentation and can be used for type-checking
 * if the project migrates to TypeScript.
 */

/**
 * Data source type for a provider.
 */
// eslint-disable-next-line no-unused-vars
const SourceType = Object.freeze({
  WebView: 'webview',
  OfficialApi: 'official_api',
  Manual: 'manual',
  ConsoleLink: 'console_link',
});

/**
 * Snapshot status values.
 */
// eslint-disable-next-line no-unused-vars
const SnapshotStatus = Object.freeze({
  Ok: 'ok',
  Warning: 'warning',
  Exhausted: 'exhausted',
  AuthError: 'auth_error',
  Authenticated: 'authenticated',
  RateLimited: 'rate_limited',
  Unsupported: 'unsupported',
  ParseError: 'parse_error',
  NetworkError: 'network_error',
});

/**
 * Panel severity levels derived from status + remainingRatio.
 */
// eslint-disable-next-line no-unused-vars
const PanelSeverity = Object.freeze({
  Normal: 'normal',
  Warning: 'warning',
  Critical: 'critical',
  Error: 'error',
});

/**
 * @typedef {Object} ProviderConfig
 * @property {string} id - Unique provider identifier (e.g. "codex", "kimi-code").
 * @property {string} name - Human-readable provider name.
 * @property {'webview'|'official_api'|'manual'|'console_link'} source - Data source type.
 * @property {string} loginUrl - Official login page URL.
 * @property {string} quotaUrl - Quota/usage page URL for extraction.
 * @property {string} consoleUrl - Provider console URL for "open in browser".
 * @property {string[]} allowedOrigins - Domain whitelist for script injection.
 * @property {string} extractorScript - JS IIFE source for quota extraction.
 */

/**
 * @typedef {Object} QuotaSnapshot
 * @property {string} providerId
 * @property {string} providerName
 * @property {'webview'|'official_api'|'manual'|'console_link'} source
 * @property {'ok'|'warning'|'exhausted'|'auth_error'|'authenticated'|'rate_limited'|'unsupported'|'parse_error'|'network_error'} status
 * @property {'normal'|'warning'|'critical'|'error'} severity
 * @property {number} remainingRatio - 0.0–1.0, or -1 if unknown.
 * @property {number} fiveHourRemainingRatio - 0.0–1.0, or -1 if unknown.
 * @property {number} used - Raw used amount, or -1.
 * @property {number} total - Raw total amount, or -1.
 * @property {string} unit - Unit label (e.g. "tokens", "credits").
 * @property {string} balanceText - Formatted balance string (e.g. "75%").
 * @property {string} fiveHourBalanceText - Formatted 5-hour balance string.
 * @property {string} resetAt - ISO 8601 timestamp for rate limit reset.
 * @property {string} updatedAt - ISO 8601 timestamp of last update.
 * @property {string} consoleUrl - URL to open provider console.
 * @property {string} message - Status or error message.
 */

/**
 * @typedef {Object} ExtractionResult
 * @property {string} providerId
 * @property {'ok'|'parse_error'} status
 * @property {number} [remainingRatio] - 0.0–1.0.
 * @property {number} [fiveHourRemainingRatio] - 0.0–1.0.
 * @property {string} [balanceText] - Formatted balance string.
 * @property {string} [fiveHourBalanceText] - Formatted 5-hour balance.
 * @property {number} [used] - Raw used amount.
 * @property {number} [total] - Raw total amount.
 * @property {string} [resetAt] - ISO 8601 reset timestamp.
 * @property {string} [message] - Error message on parse_error.
 * @property {string} updatedAt - ISO 8601 timestamp.
 */

/**
 * Bridge API exposed via QWebChannel at channel.objects.bridge.
 *
 * @typedef {Object} CodingPlanBridge
 * @property {ProviderConfig[]} providers - READ-ONLY list of providers.
 * @property {QuotaSnapshot[]} snapshots - READ-ONLY list of quota snapshots.
 * @property {function(): void} refreshAll - Trigger background refresh.
 * @property {function(providerId: string): void} refreshProvider - Refresh single provider.
 * @property {function(providerId: string): void} openConsole - Open console in browser.
 * @property {function(providerId: string): void} requestLogin - Start login flow.
 * @property {function(providerId: string): void} finishLogin - End login flow.
 * @property {function(providerId: string, ratio: number): void} setManualRatio - Set manual ratio.
 * @property {function(providerId: string, result: ExtractionResult): void} setWebViewResult - Store extraction result.
 * @property {function(providerId: string, message: string): void} setProviderError - Record error.
 * @property {function(providerId: string): ProviderConfig} getProviderConfig - Get provider definition.
 *
 * Signals (connect via bridge.signalName.connect(callback)):
 * @property {Object} dataChanged - Fired when providers or snapshots change.
 * @property {Object} loginPageRequested - Fires (providerId, loginUrl).
 * @property {Object} refreshAllRequested - Fires when refresh all is requested.
 * @property {Object} loginFinished - Fires (providerId).
 */

export { SourceType, SnapshotStatus, PanelSeverity };
