# Repository Guidelines

## Project Structure & Module Organization

This repository contains a DDE Shell coding-plan quota plugin and standalone settings app.

- `src/`: shared Qt/C++ core models, provider registry, and shell applet implementation.
- `app/`: standalone DTK/Qt WebEngine window and WebChannel bridge.
- `package/`: DDE Shell QML package files, including `main.qml`.
- `web/`: React/Vite frontend; source lives in `web/src/`.
- `tests/`: QtTest-based C++ tests for provider metadata and integration assumptions.
- `docs/`: product and design notes, currently `docs/prd.md`.

## Build, Test, and Development Commands

Run frontend commands with `--prefix web`.

- `npm --prefix web install`: install dependencies from `package-lock.json`.
- `npm --prefix web run build`: generate `web/dist`, which CMake copies and installs.
- `npm --prefix web run dev`: start the Vite dev server for frontend-only work.
- `cmake -S . -B build`: configure the Qt/CMake build.
- `cmake --build build`: build the core library, standalone app, applet, and tests.
- `ctest --test-dir build --output-on-failure`: run registered QtTest tests.

Qt6, DTK6, Qt WebEngine, and DDE Shell development packages are required for a full build.

## Coding Style & Naming Conventions

C++ targets use C++17 and Qt idioms. Keep SPDX headers on C++ files. Follow the existing brace and spacing style: return types on separate lines for definitions, two-space indentation, Qt containers, and `QStringLiteral` for stable strings. Use PascalCase for Qt classes and camelCase for methods and local variables.

React code uses ES modules, functional components, Material UI, and two-space indentation. Name components in PascalCase, hooks/helpers in camelCase, and keep route pages under `web/src/pages/`.

## Testing Guidelines

Add C++ tests under `tests/` and register them in `CMakeLists.txt` with `add_test`. Use QtTest private slots named for behavior, for example `snapshotStatusMapsToPanelSeverity`. Prefer focused assertions over broad snapshot-style checks. Run `ctest --test-dir build --output-on-failure` before submitting changes.

The frontend currently has no dedicated test runner; validate with `npm --prefix web run build` and manual checks for changed views.

## Commit & Pull Request Guidelines

Recent history uses short imperative subjects and occasional Conventional Commit prefixes, such as `fix:` and `feat:`. Keep subjects concise and specific, for example `fix: convert bridge init to awaitable singleton Promise`.

Pull requests should include a brief summary, test/build results, linked issues when applicable, and screenshots or screen recordings for UI changes in QML or React. Note any environment limits, such as missing DDE Shell SDK or Qt WebEngine runtime.
