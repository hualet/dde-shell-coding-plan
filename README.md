# dde-shell-coding-plan

[![Build](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/build.yml/badge.svg)](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/build.yml)
[![Build Debian Package](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/deb.yml/badge.svg)](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/deb.yml)

DDE Shell 插件，用来显示各类 Coding Plan 的余量

## 当前能力

- 提供 DDE Shell 插件骨架，包 ID 为 `org.deepin.ds.coding-plan`。
- 面板入口以圆环显示 Codex、Claude、Kimi Code、GLM Coding、MiniMax Coding 的额度状态。
- **直连官方 API 获取额度，无需浏览器插件**：自动识别本机已登录的 Codex CLI（`~/.codex`）、Claude Code（`~/.claude`）/ Claude 桌面版（`~/.config/Claude`）、Kimi Code CLI（`~/.kimi-code`）、ZCode/GLM（`~/.zcode`）账号；MiniMax 等无本地凭据的厂商可在面板手动添加 API Key（支持同厂商多账号）。
- Codex 访问令牌过期时自动刷新并写回 `~/.codex/auth.json`；Kimi、Claude 凭据只读（由各自 CLI 负责刷新）；未登录 Claude Code 时读取 Claude 桌面版记录的额度。
- Popup 展示各账号的状态、5 小时/周额度、控制台入口，支持删除手动账号。
- **设置面板**（任务栏插件右键"设置"为主入口，popup 右上角"设置"为辅）：「显示 Plan」逐个开关每个账户（自动识别 + 手动添加）是否显示到任务栏；「添加 Plan」管理手动账号。
- **国际化**：界面文案英文源 + `translations/` 多语言目录（zh_CN 已完整翻译），随 dde-shell 翻译机制加载。
- 内置 provider 元数据：登录 URL、控制台 URL。
- 核心模型、凭据读取和响应解析可在没有 DDE Shell SDK 的环境中单独测试。

## 构建

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

如果本机未安装 DDE Shell 开发包，CMake 会只构建 `coding-plan-core` 和单元测试；安装 DDE Shell SDK 后会同时构建 `ds-coding-plan-applet`。

> 注意：若 shell 中带有 ZCode AppImage 注入的 `LD_LIBRARY_PATH`，系统 cmake 会报
> `Could not find CMAKE_ROOT`，此时用 `env -u LD_LIBRARY_PATH cmake …` 运行上述命令。

## Debian/Deepin 打包

```bash
dpkg-buildpackage -us -uc -b
```

推送 `v*` 标签（或发布 Release）时，GitHub Actions 会自动构建
`dde-shell-coding-plan_*.deb` 并挂到对应的 Release 上，可直接下载安装。

## 文档

- [产品需求文档 PRD](docs/prd.md)
- [架构与端点说明](AGENTS.md)
