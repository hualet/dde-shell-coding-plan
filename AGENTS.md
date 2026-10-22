# Repository Guidelines

## Project Overview

DDE Shell plugin (`org.deepin.ds.coding-plan`) that displays AI coding plan quotas (Codex, Claude, Kimi Code, GLM Coding, MiniMax) in the deepin taskbar. Quotas are fetched **directly from the vendors' official usage APIs** using credentials read from local coding-agent CLI config files (the approach used by orca and magpie) — no browser extension is involved. Accounts come from two sources:

- **Auto-detected**: sign-ins of the Codex / Claude Code / Kimi Code / ZCode (GLM) CLIs found on this machine, plus Claude Desktop's on-disk usage samples.
- **Manual**: user-pasted API keys (MiniMax, GLM, Kimi Code support key-based quota queries; multiple accounts per vendor are allowed).

## Architecture

- `src/codingplanapplet.h/cpp` — DDE Shell `DApplet` subclass, entry point. Creates `CodingPlanModel`, starts auto-refresh, owns `CodingPlanSettings`, exposes `Q_INVOKABLE showSettings()` for the panel popup and the right-click context menu.
- `src/codingplanmodel.h/cpp` — QML-facing model. Tracks **entries** (auto-detected CLI accounts + manually added API-key accounts), one `QuotaSnapshot` per entry, persisted via QSettings (`snapshots`) plus a `manualAccounts` list. Manual API keys live in an owner-only JSON file (`$XDG_CONFIG_HOME/dde-shell-coding-plan/provider-keys.json`), never in QSettings. Adaptive auto-refresh: every 1 min while any quota reading changes, +30 s per round without change, capped at 5 min (failures count as no change); on a non-auth refresh failure a good snapshot whose last *success* is under 30 minutes old is kept with the error as its message (stale-keep policy; `updatedAt` is not bumped, AuthError is never masked). Refresh progress is exposed as `refreshing` / `lastRefreshAt` plus a per-snapshot `refreshing` flag (all fetches go through `startRequests`; some sources answer synchronously). Per-entry taskbar visibility ("Show Plans"/zh 显示 Plan switches) also lives here: `setEntryVisible(entryId, visible)` persists a `hiddenEntries` id list in the same QSettings (stale ids pruned when their entry disappears), snapshots carry a `visible` flag, and the tooltip skips hidden entries. Settings are written only by the model itself, so there is no file watcher. Credential detection is injectable via `setCredentialProbe` for tests.
- `src/codingplansettings.h/cpp` — DTK settings dialog (applet-side, links Dtk6 Core+Widget). `CodingPlanSettings` builds a `DSettings` schema with two groups — "Show Plans"/zh 显示 Plan (`codingplan-planlist` → `PlanVisibilityWidget`: one `DSwitchButton` per account, auto and manual, note "Plans switched on are shown on the taskbar") and "Add Plan"/zh 添加 Plan (`codingplan-accounts` → `ManualAccountsWidget`: manual account list with remove buttons + the add-account form moved out of the popup). Both widgets are pure views over `CodingPlanModel`; nothing is stored inside DSettings itself (visibility persists in the model's QSettings), so no `DSettingsBackend` is involved and the dialog's reset button is hidden. DTK schema rules baked into the design: the dialog only renders options inside a **child group** of a top group (each group uses one unnamed subgroup as the row container), and option `key` fields are short names that DSettings prefixes with the group path. Schema group names are translated by the dialog through `updateSettings(translateContext, settings)` — the context passed is `CodingPlanSettings`, matching where the schema's `tr()` calls live.
- `src/direct_quota_provider.h/cpp` — async HTTP fetcher (QNetworkAccessManager, 30 s per-request timeout). Reads credentials through `CredentialStore`, calls each vendor's usage endpoint, maps HTTP 401/403→AuthError, 429→RateLimited; connection errors and timeouts may fail over to a second host. A refresh of an entry whose request is still pending is ignored (protects the rotating Codex refresh token). Also owns the Codex OAuth token refresh (POST `auth.openai.com/oauth/token` with the Codex CLI client id) and writes rotated tokens atomically back to `~/.codex/auth.json`, preserving all other fields. It refuses to refresh when `auth.json` is not writable, and a failed write-back or a 4xx from the token endpoint is an AuthError.
- `src/credential_store.h/cpp` — read-only access to CLI credential files: `$CODEX_HOME/auth.json`, `$CLAUDE_CONFIG_DIR/.credentials.json` (default `~/.claude/...`), `$XDG_CONFIG_HOME/Claude/plan-usage-history.json` (Claude Desktop), `$KIMI_CODE_HOME/credentials/kimi-code.json` (default `~/.kimi-code/...`), `~/.zcode/v2/config.json` (+ `setting.json` to find the active `builtin:*-coding-plan` provider). **Read-only policy**: only Codex tokens are ever written back.
- `src/quota_parsers.h/cpp` — pure JSON→`QuotaSnapshot` parsers per vendor (unit-tested against fixture replies). Shared helpers: used-percent→remaining-ratio clamping, epoch seconds/ms adaptation (1e10 threshold), JWT exp decoding.
- `src/providerregistry.h/cpp` — static provider definitions (`codex`, `claude`, `kimi-code`, `glm-coding`, `minimax`) with `loginUrl`, `consoleUrl`, `SourceType::OfficialApi`. Also defines `QuotaSnapshot`, `SnapshotStatus`, `PanelSeverity` enums.
- `package/main.qml` — panel ring + popup UI. Per-account cards: 5-hour/weekly quota progress bars, plus a lean text line showing the update time, or for entries that fail to refresh a one-line `Cannot update: <reason>` instead (manual accounts show a label and a remove button); no WebEngine. Taskbar rings show exactly the plans switched on in the settings dialog (`taskbarSnapshots` filters snapshots by their `visible` flag); the tooltip and the popup follow the same filter. Settings entry: right-click context menu (`LP.Menu`, the main entry) plus an icon-only gear button (`DciIcon` `preferences-system`) in the popup header, both calling `Applet.showSettings()`.

### Vendor endpoints & credentials

| Provider | Credentials | Endpoint |
|---|---|---|
| codex (auto) | `~/.codex/auth.json` → `tokens.{access_token, refresh_token, account_id}` | `GET https://chatgpt.com/backend-api/wham/usage` with `Authorization: Bearer`, `chatgpt-account-id`, `OpenAI-Beta: responses=experimental`, `originator: codex_cli_rs`, UA `codex_cli_rs/…` |
| claude (auto, CLI) | `~/.claude/.credentials.json` → `claudeAiOauth.{accessToken, expiresAt(ms), subscriptionType}` (read-only; expired → ask the user to rerun claude, or fall back to Desktop) | `GET https://api.anthropic.com/api/oauth/usage` with Bearer + `anthropic-beta: oauth-2025-04-20`; `five_hour`/`seven_day.{utilization %, resets_at}` |
| claude (auto, Desktop fallback) | none — `~/.config/Claude/plan-usage-history.json`, latest `samples[].u.{fh, sd}` (5h / 7d used %, no reset time); `updatedAt` = sample time `t`, flagged stale after 1 h | no network |
| kimi-code (auto) | `~/.kimi-code/credentials/kimi-code.json` → `access_token` (read-only; expired → ask the user to rerun the kimi CLI) | `GET https://api.kimi.com/coding/v1/usages` with Bearer |
| kimi-code (manual) | user API key | same endpoint, Bearer key |
| glm-coding (auto) | `~/.zcode/v2/config.json` → active `builtin:*-coding-plan` `options.apiKey`; host from its `baseURL` | `GET {root}/api/monitor/usage/quota/limit` with the **bare key** in `Authorization` (no Bearer), `Accept-Language: en-US,en` |
| glm-coding (manual) | user API key | same endpoint, default host `open.bigmodel.cn`, failover `api.z.ai` (same path) on network errors/timeouts only |
| minimax (manual) | user API key | `GET https://api.minimaxi.com/v1/token_plan/remains` with Bearer (HTTP 200 + `base_resp.status_code==1004` means auth failure) |

Provider IDs `"codex"`, `"claude"`, `"kimi-code"`, `"glm-coding"`, `"minimax"` are used identically across model, fetcher and registry.

### Internationalization

- User-visible strings use **English source text** wrapped in `tr()`/`qsTr()` (QObject classes use their own context; the free functions in `quota_parsers.cpp` go through a `ParserTr` shim with `Q_DECLARE_TR_FUNCTIONS`, context `QuotaParsers`; QML strings land in the `main` context). Never put Chinese (or any other language) directly in string literals — a contract test fails on CJK inside literals.
- `translations/org.deepin.ds.coding-plan_<locale>.ts` carry the translations; `zh_CN` is fully translated (the shipped Chinese UI text), other locale files are generated empty skeletons for future translators.
- CMake wires `ds_handle_package_translation` (Qt `LinguistTools`): lupdate/lrelease run at configure/build time and `.qm` catalogs install to `<package>/translations/`, where dde-shell's `DAppletLoaderPrivate::loadTranslation` picks up `<pluginId>_<locale>.qm` and installs it on `qApp` — one catalog covers both C++ `tr()` and QML `qsTr()`.
- After adding/changing user-visible strings, re-run cmake configure (or the `update_translations` target) so lupdate refreshes the `.ts` files, then fill the new `<unfinished>` entries in `zh_CN`.

## Build & Test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

- Requires: Qt6 (Core, Gui, **Network**, Quick, QuickControls2, Test), DTK6, DDE Shell SDK.
- Without DDE Shell SDK: only `coding-plan-core` static lib and tests build (applet is skipped).
- **On machines where the ZCode AppImage injects its `LD_LIBRARY_PATH`**, system cmake breaks (`Could not find CMAKE_ROOT`); run build/test commands as `env -u LD_LIBRARY_PATH cmake …`.

### Running a single test

```bash
./build/providerregistry_tests snapshotStatusMapsToPanelSeverity
```

Pass the slot name as a positional argument to the test binary.

## Testing Conventions

- Tests live in `tests/providerregistry_test.cpp`, registered via `add_test` in `CMakeLists.txt`.
- Parser/credential tests are real unit tests: fixture JSON / temp files in, asserted `QuotaSnapshot` fields out. Structural/contract assertions (endpoints present in source, QML references, CMake contents) complement them. When adding features, add corresponding tests of both kinds.
- Model tests sandbox persistence: `initTestCase` redirects QSettings (`QSettings::setPath`) and `XDG_CONFIG_HOME` into a `QTemporaryDir`; tests needing isolation call `resetPersistence()` first and use `setCredentialProbe` instead of touching real credential files.
- `SOURCE_DIR` is injected via `target_compile_definitions` — use `QStringLiteral(SOURCE_DIR "/...")` for file paths in tests.
- Test class name: `ProviderRegistryTest`. Test methods are private slots with descriptive camelCase names.
- Known toolchain quirk: moc (this Qt build) fails with `missing ')' in macro usage` on `QStringLiteral (` followed by a **newline and** a raw string literal — keep raw strings as direct call arguments within one statement.

## Coding Style

- C++17, Qt idioms. SPDX headers on all files.
- Return types on separate lines for definitions. Two-space indentation.
- `QStringLiteral` for stable strings. Qt containers over STL where Qt API is used.
- PascalCase for classes, camelCase for methods/variables.
- `Q_PROPERTY` with space before parenthesis: `Q_PROPERTY (Type *name READ name NOTIFY nameChanged)`.

## Verification Checklist

After any change:

1. `env -u LD_LIBRARY_PATH cmake -S . -B build && env -u LD_LIBRARY_PATH cmake --build build` (drop the `env -u` if your shell doesn't carry the injected `LD_LIBRARY_PATH`)
2. `env -u LD_LIBRARY_PATH ctest --test-dir build --output-on-failure`

For changes to endpoints, credential paths, auth or parsing (`credential_store.*`, `quota_parsers.*`, `direct_quota_provider.*`): update the fixture tests so the table above and the tests stay in sync.

If environment deps are missing and a command fails, report the exact command, failure reason, and remaining risk.

## Packaging & CI

- `.github/workflows/build.yml` — push/PR CI: configure + build + `ctest` inside the `docker.io/hualet/deepin:latest-builder` container, then upload the staged install tree as an artifact.
- `.github/workflows/deb.yml` — on `v*` tag push or a published release: builds `dde-shell-coding-plan_<ver>_<arch>.deb` with `dpkg-buildpackage -us -uc -b` and attaches it to the GitHub release (`softprops/action-gh-release`).
- `debian/` — debhelper 13, quilt source format. Binary package `dde-shell-coding-plan` depends on `dde-shell` plus `${shlibs:Depends}`; `debian/rules` is the standard dh + CMake wrapper with hardening. No `postinst` — the plugin is picked up after the next dde-shell restart.
- Release flow: bump `project()` in `CMakeLists.txt`, add a matching `debian/changelog` entry (`<version>-1`), commit, tag `v<version>`, push — the deb workflow publishes the .deb. Keep the changelog version and the CMake version in sync.
- Local package build check: `env -u LD_LIBRARY_PATH dpkg-buildpackage -us -uc -b` (artifacts land in the parent directory; they are gitignored).

## Commit Style

Short imperative subjects. Conventional Commit prefixes when applicable (`feat:`, `fix:`, `docs:`, `refactor:`).
