# dde-shell-coding-plan
DDE Shell 插件，用来显示各类 Coding Plan 的余量

## 当前能力

- 提供 DDE Shell 插件骨架，包 ID 为 `org.deepin.ds.coding-plan`。
- 面板入口以圆环显示 Codex、Kimi Code、GLM Coding、MiniMax Coding 的额度状态。
- Popup 展示各 provider 的状态、来源、登录入口、控制台入口、刷新入口和手动录入降级。
- 内置 provider 元数据：登录 URL、额度/控制台 URL、允许域名。
- 通过 Chrome Extension (MV3) 在浏览器后台加载额度页、提取用量数据，经 WebSocket 回传给 DDE Shell 插件。
- 核心模型和 provider registry 可在没有 DDE Shell SDK 的环境中单独测试。

## 构建

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

如果本机未安装 DDE Shell 开发包，CMake 会只构建 `coding-plan-core` 和单元测试；安装 DDE Shell SDK 后会同时构建 `ds-coding-plan-applet`。

## Chrome Extension

将 `extension/` 目录作为未打包扩展加载到 `chrome://extensions`（开发者模式）。修改 provider 提取逻辑后需重新生成 tab extractor：

```bash
node extension/scripts/generate-tab-extractor.js
```

## 文档

- [产品需求文档 PRD](docs/prd.md)
