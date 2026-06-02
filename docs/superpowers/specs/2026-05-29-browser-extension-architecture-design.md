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

## 7. Error Handling

| Scenario | Status | Behavior |
|----------|--------|----------|
| User not logged in | `auth_error` | Show "login needed" message, preserve last good data. |
| Page parse fails | `parse_error` | Retry up to 6 times with 1.2s delay, then show error. |
| Network error | `network_error` | Show error, preserve last good data. |
| Rate limited | `rate_limited` | Show error with retry suggestion. |
| Extraction returns null | `parse_error` | Same as parse fail: retry then error. |
| Already logged in but no quota | `authenticated` | Show "waiting for quota" message. |

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
