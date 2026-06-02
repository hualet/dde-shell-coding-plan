# Repository Guidelines

## Project Structure & Module Organization

This repository contains a DDE Shell coding-plan quota plugin that communicates with a Chrome Extension via WebSocket.

- `src/`: shared Qt/C++ core models, provider registry, WebSocket server, browser extension provider, and shell applet implementation.
- `package/`: DDE Shell QML package files, including `main.qml`.
- `extension/`: Chrome Extension (MV3) source code - service worker, offscreen document, providers, popup.
- `tests/`: QtTest-based C++ tests for provider metadata and integration assumptions.
- `docs/`: product and design notes, including `docs/prd.md` and `docs/superpowers/specs/`.

## Architecture

The plugin uses a WebSocket server (127.0.0.1:18765) to communicate with a Chrome Extension. The Extension loads provider quota pages in offscreen documents, extracts quota data, and sends results back via WebSocket. No Qt WebEngine dependency is needed.

- `src/websocket_server.h/cpp`: WebSocket server handling auth, message routing.
- `src/browser_ext_provider.h/cpp`: Provider logic that sends refresh requests and processes results.
- `extension/`: Chrome Extension with service worker, offscreen extraction, and per-provider JS extractors.

## Build, Test, and Development Commands

- `cmake -S . -B build`: configure the Qt/CMake build.
- `cmake --build build`: build the core library, applet, and tests.
- `ctest --test-dir build --output-on-failure`: run registered QtTest tests.

Qt6, DTK6, Qt WebSockets, and DDE Shell development packages are required for a full build.

## Required Verification

After any code change, run the relevant compile and unit-test commands before handing off the work. For C++/Qt/QML changes, run `cmake -S . -B build`, `cmake --build build`, and `ctest --test-dir build --output-on-failure`. For Chrome Extension changes, there is no build step; validate affected files by loading `extension/` as an unpacked extension, and still run the C++ build/test commands when the change touches WebSocket protocol, provider contracts, or C++ integration. If an environment dependency prevents a command or manual check from running, report the exact command/check, failure reason, and remaining risk in the final handoff.

### Chrome Extension Development

- Load `extension/` as an unpacked extension in `chrome://extensions` (developer mode).
- No build step required - the extension uses plain ES modules.

## Coding Style & Naming Conventions

C++ targets use C++17 and Qt idioms. Keep SPDX headers on C++ files. Follow the existing brace and spacing style: return types on separate lines for definitions, two-space indentation, Qt containers, and `QStringLiteral` for stable strings. Use PascalCase for Qt classes and camelCase for methods and local variables.

Extension JS uses ES modules, camelCase for functions/variables, PascalCase for provider objects.

## Testing Guidelines

Add C++ tests under `tests/` and register them in `CMakeLists.txt` with `add_test`. Use QtTest private slots named for behavior, for example `snapshotStatusMapsToPanelSeverity`. Prefer focused assertions over broad snapshot-style checks. Run `ctest --test-dir build --output-on-failure` before submitting changes.

## Commit & Pull Request Guidelines

Recent history uses short imperative subjects and occasional Conventional Commit prefixes, such as `fix:` and `feat:`. Keep subjects concise and specific.

Pull requests should include a brief summary, test/build results, linked issues when applicable, and screenshots or screen recordings for UI changes in QML. Note any environment limits, such as missing DDE Shell SDK.
