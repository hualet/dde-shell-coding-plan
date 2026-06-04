# dde-shell-coding-plan
DDE Shell 插件，用来显示各类 Coding Plan 的余量

## 当前能力

- 提供 DDE Shell 插件骨架，包 ID 为 `org.deepin.ds.coding-plan`。
- 面板入口以圆环显示 Codex、Kimi Code、GLM Coding、MiniMax Coding 的额度状态。
- Popup 展示各 provider 的状态、来源、登录入口、控制台入口、刷新入口和手动录入降级。
- 内置 WebView provider 元数据：登录 URL、额度/控制台 URL、允许域名、受控读取脚本。
- 通过 `ProviderWebView.qml` 在插件内加载官方登录/额度页；运行环境需要 Qt WebEngine QML 模块。
- 核心模型和 provider registry 可在没有 DDE Shell SDK 的环境中单独测试。

> Codex、Kimi Code、GLM Coding、MiniMax Coding 的真实额度页字段仍需要使用真实账号验证。当前实现会在未建立 WebView 登录会话时明确提示登录，并保留手动录入/控制台跳转作为 MVP 降级路径。

## 构建

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

如果本机未安装 DDE Shell 开发包，CMake 会只构建 `coding-plan-core` 和单元测试；安装 DDE Shell SDK 后会同时构建 `ds-coding-plan-applet`。运行内置登录页还需要安装 Qt WebEngine QML 运行时。

## 文档

- [产品需求文档 PRD](docs/prd.md)
