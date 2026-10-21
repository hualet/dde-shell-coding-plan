# DDE Shell Coding Plan Plugin

[![Build](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/build.yml/badge.svg)](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/build.yml)
[![Build Debian Package](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/deb.yml/badge.svg)](https://github.com/hualet/dde-shell-coding-plan/actions/workflows/deb.yml)

Deepin DDE Shell 的任务栏 Coding Plan 插件，使用 Qt6/C++/QML 构建，集中显示 Codex、Claude、Kimi Code、GLM Coding、MiniMax 等订阅的剩余额度，直连各厂商官方用量 API，无需浏览器插件。

![plugin showcase](./docs/showcases/showcase.png)

## 功能

- 任务栏以圆环常驻显示各 Coding Plan 的额度状态，悬停提示、点击弹窗查看详情
- 自动识别本机已登录的 CLI 账号：Codex CLI（`~/.codex`）、Claude Code（`~/.claude`）/ Claude 桌面版（`~/.config/Claude`）、Kimi Code CLI（`~/.kimi-code`）、ZCode/GLM（`~/.zcode`）
- MiniMax、GLM、Kimi Code 支持手动添加 API Key 账号，同厂商可添加多个
- Codex 访问令牌过期时自动刷新并写回 `~/.codex/auth.json`；其余 CLI 凭据只读，由各自 CLI 负责刷新
- 弹窗展示各账号的状态、5 小时/周额度与控制台入口，支持删除手动账号
- 设置面板：逐个开关账户是否显示到任务栏、管理手动账号（任务栏插件右键“设置”为主入口）
- 自适应自动刷新：额度变化快时每分钟一轮，无变化时逐步放慢，最长 5 分钟
- 国际化：英文源文案 + `translations/` 多语言目录，zh_CN 已完整翻译
- 核心模型、凭据读取与响应解析可在没有 DDE Shell SDK 的环境中单独测试

## 快速开始

### 构建

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

> 注意：若 shell 中带有 ZCode AppImage 注入的 `LD_LIBRARY_PATH`，系统 cmake 会报
> `Could not find CMAKE_ROOT`，此时用 `env -u LD_LIBRARY_PATH cmake …` 运行上述命令。

如果本机未安装 DDE Shell 开发包，CMake 会只构建 `coding-plan-core` 和单元测试；安装 DDE Shell SDK 后会同时构建 `ds-coding-plan-applet`。

### 安装

```bash
sudo cmake --install build
```

安装后重启 dde-shell 即可在任务栏看到插件。

### Debian/Deepin 打包

```bash
dpkg-buildpackage -us -uc -b
```

推送 `v*` 标签（或发布 Release）时，GitHub Actions 会自动构建
`dde-shell-coding-plan_*.deb` 并挂到对应的 Release 上，可直接下载安装。

## 文档

- [产品需求文档 PRD](docs/prd.md)
- [架构与端点说明](AGENTS.md)

## 技术概览

- 后端：C++17、Qt6 Network
- 前端：QML、Qt Quick Controls 2、DTK6
- 运行模式：DDE Shell 插件
- 数据源：OpenAI（Codex）、Anthropic（Claude）、Kimi、智谱（GLM Coding）、MiniMax 的官方用量 API

项目主要目录：

```text
.
├── src/           C++ 后端与插件入口
├── package/       QML 界面与插件元数据
├── translations/  多语言目录（zh_CN 已完整翻译）
├── tests/         单元测试与契约测试
├── debian/        Debian/Deepin 打包配置
└── docs/          产品与开发文档
```

## 适用环境

- CMake >= 3.16
- Qt6（Core、Gui、Network、Quick、QuickControls2、Test）
- DTK6
- DDE Shell 开发包（可选，缺失时仅构建核心库与测试）
- 支持 C++17 的编译器

## 贡献

欢迎提交 Issue 和 Pull Request。

## 许可证

GPL-3.0-or-later，applet、设置对话框与 QML 界面等部分源文件为 LGPL-3.0-or-later，以各文件的 SPDX 标识为准。

## 致谢

- [DDE Shell](https://github.com/linuxdeepin/dde-shell)
- 额度直连官方 API 的方案参考了 orca 与 magpie 的做法
