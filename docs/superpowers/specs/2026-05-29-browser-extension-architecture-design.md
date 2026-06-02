# Browser Extension Architecture Design

> Date: 2026-05-29
> Status: Active
> Scope: dde-shell-coding-plan plugin

## 1. Overview

This document defines the Native/Web boundary, Bridge protocol, data flow, and module responsibilities for the Coding Plan quota plugin. The plugin runs in two contexts:

1. **DDE Shell Panel Applet** — QML-based panel widget showing quota rings, with a popup that embeds a WebView for login/extraction.
2. **Standalone Settings Window** — DTK/Qt WebEngine desktop window hosting a React frontend, with a QWebChannel Bridge for data exchange.

Both contexts share the same core library (`coding-plan-core`): `ProviderRegistry`, `CodingPlanModel`, and the `QuotaSnapshot` data model.

## 2. Native/Web Boundary

```
┌─────────────────────────────────────────────────────┐
│                   Native Layer (C++)                 │
│                                                      │
│  ProviderRegistry  →  CodingPlanModel               │
│  (provider defs)      (snapshots, refresh, persist)  │
│         │                      │                     │
│         └──────────┬───────────┘                     │
│                    │                                  │
│  ┌─────────────────┴──────────────────┐              │
│  │           Bridge Layer             │              │
│  │  Standalone: WebBridge (QWebChannel)│              │
│  │  Panel: QML direct property access │              │
│  └─────────────────┬──────────────────┘              │
│                    │                                  │
│  ┌─────────────────┴──────────────────┐              │
│  │         WebView / Frontend         │              │
│  │  Panel: ProviderWebView.qml        │              │
│  │  Standalone: React + bridge.js     │              │
│  └────────────────────────────────────┘              │
└─────────────────────────────────────────────────────┘
```

### 2.1 Native Responsibilities

| Module | Responsibility |
|--------|---------------|
| `ProviderRegistry` | Define provider list (id, name, URLs, allowed origins, extractor script). Immutable at runtime. |
| `CodingPlanModel` | Manage `QuotaSnapshot` lifecycle: create, load, save, update, notify changes. Handle background refresh timer. |
| `WebBridge` (standalone) | Expose `CodingPlanModel` to React frontend via QWebChannel. Translate signals. |
| `CodingPlanApplet` (panel) | Expose `CodingPlanModel` to QML via Qt property system. Launch standalone settings. |
| `MainWindow` (standalone) | Manage QWebEngineView lifecycle, navigate login→quota→extract flow, run extractor scripts. |
| `ProviderWebView.qml` (panel) | Embed QML WebEngineView, manage login state machine, run extractor scripts. |

### 2.2 Web/Frontend Responsibilities

| Module | Responsibility |
|--------|---------------|
| `bridge.js` | Wrap QWebChannel calls; provide mock data for standalone dev mode. |
| `App.jsx` + pages | Render provider list, quota details, login flow. Consume Bridge API only. |
| `main.qml` | Render panel rings, status popup, web popup. Access model via `Applet.quota` property. |

### 2.3 Boundary Rules

1. Web layer **never** directly accesses provider extractor scripts, login URLs, or allowed origins — it reads them from `providers` property exposed by the Bridge.
2. Web layer **never** directly persists data — all mutations go through Bridge methods (`setManualRatio`, `setWebViewResult`, `setProviderError`).
3. Native layer **never** renders UI — it only provides data and methods.
4. Extractor scripts are **owned by Native** and injected at runtime by the WebView host (QML `runJavaScript` or C++ `runJavaScript`).
5. The `allowedOrigins` whitelist is enforced at the Native/WebView boundary, not in the frontend.

## 3. Bridge Protocol

### 3.1 Properties (read-only from Web)

| Property | Type | Description |
|----------|------|-------------|
| `providers` | `ProviderConfig[]` | List of registered providers with URLs and metadata. |
| `snapshots` | `QuotaSnapshot[]` | Current quota state for all providers. |

### 3.2 Methods (Web → Native)

| Method | Parameters | Returns | Description |
|--------|-----------|---------|-------------|
| `refreshAll()` | — | void | Trigger background refresh of all providers. |
| `refreshProvider(providerId)` | `string` | void | Refresh a single provider. |
| `openConsole(providerId)` | `string` | void | Open provider console URL in system browser. |
| `requestLogin(providerId)` | `string` | void | Navigate WebView to provider login page. |
| `finishLogin(providerId)` | `string` | void | Signal that login flow is complete. |
| `setManualRatio(providerId, ratio)` | `string, number` | void | Manually set remaining ratio (0.0–1.0). |
| `setWebViewResult(providerId, result)` | `string, object` | void | Store extraction result from WebView. |
| `setProviderError(providerId, message)` | `string, string` | void | Record extraction failure. |
| `getProviderConfig(providerId)` | `string` | `ProviderConfig` | Get full provider definition (used internally by Native WebView host). |

### 3.3 Signals (Native → Web)

| Signal | Payload | Description |
|--------|---------|-------------|
| `dataChanged` | — | Fired when `providers` or `snapshots` change. |
| `loginPageRequested(providerId, loginUrl)` | `string, string` | Standalone: navigate WebView to login URL. |
| `refreshAllRequested()` | — | Standalone: start sequential provider refresh. |
| `loginFinished(providerId)` | `string` | Standalone: login/extraction flow completed. |
| `snapshotsChanged()` | — | Panel: snapshots updated. |
| `backgroundRefreshRequested()` | — | Panel: timer-triggered background refresh. |
| `sessionCleared(providerId)` | `string` | Panel: provider session cleared. |

## 4. Type Definitions

### 4.1 ProviderConfig

```typescript
type ProviderConfig = {
  id: string;
  name: string;
  source: "webview" | "official_api" | "manual" | "console_link";
  loginUrl: string;
  quotaUrl: string;
  consoleUrl: string;
  allowedOrigins: string[];
  extractorScript: string;
};
```

### 4.2 QuotaSnapshot

```typescript
type QuotaSnapshot = {
  providerId: string;
  providerName: string;
  source: "webview" | "official_api" | "manual" | "console_link";
  status: "ok" | "warning" | "exhausted" | "auth_error" | "authenticated"
        | "rate_limited" | "unsupported" | "parse_error" | "network_error";
  severity: "normal" | "warning" | "critical" | "error";
  remainingRatio: number;       // 0.0–1.0, -1 if unknown
  fiveHourRemainingRatio: number; // 0.0–1.0, -1 if unknown
  used: number;                 // -1 if unknown
  total: number;                // -1 if unknown
  unit: string;
  balanceText: string;
  fiveHourBalanceText: string;
  resetAt: string;              // ISO 8601
  updatedAt: string;            // ISO 8601
  consoleUrl: string;
  message: string;
};
```

### 4.3 ExtractionResult (WebView → Native)

```typescript
type ExtractionResult = {
  providerId: string;
  status: "ok" | "parse_error";
  remainingRatio?: number;
  fiveHourRemainingRatio?: number;
  balanceText?: string;
  fiveHourBalanceText?: string;
  used?: number;
  total?: number;
  resetAt?: string;
  message?: string;
  updatedAt: string;
};
```

## 5. Data Flow

### 5.1 Login & Extract Flow (Standalone)

```
React UI → bridge.requestLogin(providerId)
  → WebBridge emits loginPageRequested(providerId, loginUrl)
  → MainWindow navigates WebView to loginUrl
  → User logs in on official page
  → onPageLoadFinished detects navigation to quota page
  → MainWindow.runExtractorWithRetry(extractorScript)
  → JS result → WebBridge.setWebViewResult(providerId, result)
  → CodingPlanModel updates snapshot → emits snapshotsChanged
  → WebBridge emits dataChanged → React re-renders
```

### 5.2 Login & Extract Flow (Panel)

```
QML popup → root.openLoginCenter(provider)
  → webPopup opens, ProviderWebView.qml loads
  → startAutoExtract() navigates to loginUrl
  → ProviderWebView._onNavigationFinished detects quota page
  → _runExtraction() → runJavaScript(extractorScript)
  → extracted(result) signal → Applet.quota.setWebViewResult(providerId, result)
  → CodingPlanModel updates snapshot → snapshotsChanged
  → QML re-renders rings and cards
```

### 5.3 Background Refresh (Panel)

```
CodingPlanModel timer (5 min) → backgroundRefreshRequested()
  → main.qml._startBgRefreshQueue()
  → For each provider with ok/warning/exhausted status:
    → bgLoader loads ProviderWebView.qml in hidden mode
    → startAutoExtract() → extract → setWebViewResult
    → Next provider in queue
```

## 6. Security Constraints

1. **Origin whitelist**: Extractor scripts only run on `allowedOrigins` domains. enforced by both C++ (`isAllowedOrigin`) and QML (`isAllowedOrigin`).
2. **No arbitrary local execution**: WebChannel does not expose `runCommand`, `readFile`, `writeFile`, or any general-purpose IPC.
3. **Session isolation**: Each provider gets an independent `QWebEngineProfile` with unique `storageName`.
4. **No credential logging**: `qWarning` statements log status/ratio only, never full tokens or cookies.
5. **Session cleanup**: On provider deletion, profile and credentials are cleared.

### 6.1 ProviderWebView Origin Guard Design

**Conclusion: Both paths enforce origin whitelisting.**

- **Standalone** (`app/main.cpp`): `isAllowedOrigin()` checks `m_loginProviderConfig["allowedOrigins"]` at line 315-320. Extraction is blocked if the current page origin is not in the whitelist. This guard runs in `onPageLoadFinished()` before any `runJavaScript()` call.
- **Panel** (`package/ProviderWebView.qml`): `isAllowedOrigin()` checks `root.allowedOrigins` at line 33-42. `_runExtraction()` (line 168-174) calls `isAllowedOrigin()` as its first check and immediately emits `extractionFailed()` with an error message if the check fails.
- **Boundary rule**: The origin check is enforced at the Native/WebView boundary (the host that runs `runJavaScript`), not in the extractor script itself and not in the React frontend.

### 6.2 OAuth Redirect State Machine Design

**Conclusion: Current implementation is safe but conservative; explicit redirect tracking is not required for MVP.**

When a provider uses OAuth-based login (e.g., Codex via `auth.openai.com`), the WebView navigates through redirect chains before landing back on the target domain:

1. **Standalone** (`main.cpp`): `onPageLoadFinished()` checks `isAllowedOrigin()` first. If the OAuth redirect page is not in `allowedOrigins`, the handler returns early without extracting. This is correct behavior — the state machine should not act on intermediate OAuth pages. When the browser eventually redirects back to the allowed origin (e.g., `chatgpt.com`), `onPageLoadFinished()` resumes processing.

2. **Panel** (`ProviderWebView.qml`): `_onNavigationFinished()` similarly checks `isAllowedOrigin()` before advancing the state machine. The `loginCheckTimer` (2.5s interval) probes the page state periodically, which naturally handles redirect delays.

3. **Risk**: If an OAuth flow takes longer than the retry window, the state machine may time out. The current 6-retry × 1.2s mechanism provides ~7.2 seconds of tolerance, which is sufficient for normal OAuth flows.

4. **Design decision**: No explicit redirect URL tracking is needed. The state machine treats any non-allowed-origin page as "still loading" and waits. This is the correct security posture — we should never inject scripts into OAuth intermediate pages.

### 6.3 localStorage/Cookie Extractor Access Design

**Conclusion: Extractor scripts access page-local storage on allowed origins only. This is an intentional design choice, documented here for security review.**

Two extractor scripts read browser-local credentials:

- **Kimi Code** (`kimiCodeExtractorScript()`): Reads `localStorage.getItem('access_token')` and uses it in a synchronous XHR to `/apiv2/kimi.gateway.billing.v1.BillingService/GetUsages`. This runs in the context of `kimi.com`, where the access token was set by Kimi's own frontend. The XHR is same-origin. This is equivalent to what the browser's own developer console would do.

- **GLM Coding** (`glmCodingExtractorScript()`): Reads `document.cookie` to extract `bigmodel_token_production`, then uses it in a synchronous XHR to `/api/monitor/usage/quota/limit`. Same-origin context on `bigmodel.cn`.

**Security properties**:
1. These scripts only run after `isAllowedOrigin()` passes — they cannot execute on arbitrary pages.
2. The tokens/cookies are never sent to any domain other than the one they originated from (same-origin XHR).
3. The tokens/cookies are never logged, stored outside the WebView profile, or transmitted to the plugin's Native layer.
4. Each provider has an isolated WebView profile, so one provider's cookies/localStorage are not accessible to another.
5. This is the same security model as a browser extension content script operating on declared host permissions.

## 7. Error Handling

| Scenario | Status | Behavior |
|----------|--------|----------|
| User not logged in | `auth_error` | Show "login needed" message, preserve last good data. |
| Page parse fails | `parse_error` | Retry up to 6 times with 1.2s delay, then show error. |
| Network error | `network_error` | Show error, preserve last good data. |
| Rate limited | `rate_limited` | Treated as "authenticated but currently unavailable". `isAuthenticated()` returns true; `isUsable()` returns false. Show "rate limited" message with retry suggestion. |
| Extraction returns null | `parse_error` | Same as parse fail: retry then error. |
| Already logged in but no quota | `authenticated` | Show "waiting for quota" message. |

### 7.1 Status Semantics: `isAuthenticated` vs `isUsable`

The Web layer provides two helper functions to distinguish between authentication state and usability:

- **`isAuthenticated(snapshot)`**: Returns true if the user has completed login at least once. Covers `ok`, `warning`, `exhausted`, `authenticated`, and `rate_limited`. Used for UI decisions like showing the quota detail page vs. the login page.
- **`isUsable(snapshot)`**: Returns true only if the provider has fresh, usable quota data. Covers `ok`, `warning`, `exhausted`. Used for display decisions like showing the green "已登录" chip vs. the yellow "受限" chip.
- **`isLoggedIn(snapshot)`**: Legacy alias for `isAuthenticated()`. Kept for backward compatibility.

### 7.2 `setWebViewResult()` Status Handling

`setWebViewResult()` reads the `status` field from the extraction result:
- If `status === "parse_error"`, the snapshot is set to `SnapshotStatus::ParseError` with the result's `message`.
- Otherwise (empty, `"ok"`, or any other value), the snapshot is set to `SnapshotStatus::Ok`.

The standalone `MainWindow` already gates `setWebViewResult()` behind a `status === "ok"` check before calling it, so `parse_error` results typically reach `setProviderError()` instead. The status read in `setWebViewResult()` is a defense-in-depth measure for the panel path and any future callers.

## 8. Module File Map

```
src/
  providerregistry.h/.cpp   — ProviderDefinition, QuotaSnapshot, ProviderRegistry
  codingplanmodel.h/.cpp    — CodingPlanModel (data + persistence + refresh)
  codingplanapplet.h/.cpp   — DDE Shell applet entry point

app/
  main.cpp                  — Standalone DTK window + WebView host
  webbridge.h/.cpp          — QWebChannel bridge for standalone

package/
  main.qml                  — Panel widget (rings + popups)
  ProviderWebView.qml       — Embedded WebView for panel popup
  metadata.json             — DDE Shell plugin metadata

web/src/
  bridge.js                 — QWebChannel client + mock data
  App.jsx                   — React app shell
  pages/AgentList.jsx       — Provider list page
  pages/LoginPage.jsx       — Login flow page
  pages/QuotaPage.jsx       — Quota detail page
  components/SlideTransition.jsx
  theme.js
  main.jsx
```

## 9. Testing Strategy

- **C++ unit tests** (`tests/providerregistry_test.cpp`): Validate provider definitions, snapshot serialization, severity mapping, extraction script content, and QML/JS source file assertions.
- **React build check**: `npm --prefix web run build` ensures frontend compiles.
- **Manual verification**: WebView login and extraction tested against live provider pages.

## 10. Extension Points

1. **Add new WebView provider**: Add entry in `ProviderRegistry::createDefault()` with id, name, URLs, allowedOrigins, and extractorScript. Update `MOCK_PROVIDERS`/`MOCK_SNAPSHOTS` in `bridge.js`.
2. **Add official_api provider**: Add entry with `SourceType::OfficialApi`, implement `fetchQuota()` in a future `ApiProvider` class.
3. **Custom extractor**: Extractor scripts are self-contained JS IIFEs that return an `ExtractionResult`. They can use DOM reading, XHR to same-origin APIs, or `localStorage` tokens.
