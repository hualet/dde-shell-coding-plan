# Browser Extension Architecture Design

日期：2026-05-29

## 1. 背景与动机

当前 dde-shell-coding-plan 使用内嵌 Qt WebEngine WebView 爬取无公开 API 的平台额度页面。存在两个核心问题：

1. **Qt WebEngine 依赖沉重**：构建复杂，运行时资源占用高，在某些 deepin 环境下有兼容性问题。
2. **登录态无法复用**：用户日常在浏览器中已登录各平台，但 WebView 是独立的 session profile，需要重新登录。

本方案将 WebView 爬取能力迁移到 Chrome Extension，DDE Shell 插件通过本地 WebSocket 与之通信，彻底移除 Qt WebEngine 依赖。

## 2. 整体架构

```
┌──────────────────────────────┐       ┌─────────────────────────────────┐
│       DDE Shell 插件 (C++)    │       │    Chrome Extension (MV3)       │
│                               │       │                                  │
│  ┌─────────────────────┐     │       │  ┌────────────────────┐         │
│  │ WebSocket Server     │◄────┼──WS──►│ Service Worker       │         │
│  │ (127.0.0.1:18765)   │     │       │  │ - WS client         │         │
│  └─────────────────────┘     │       │  │ - 刷新调度           │         │
│                               │       │  └────────────────────┘         │
│  QuotaProviderRegistry       │       │  ┌────────────────────┐         │
│  QuotaSnapshot 数据消费      │       │  │ Offscreen Document  │         │
│  面板圆环 + Popup            │       │  │ - 加载额度页面       │         │
│                               │       │  │ - 注入提取脚本       │         │
│  official_api provider       │       │  │ - 串行处理多平台     │         │
│  manual provider             │       │  └────────────────────┘         │
│                               │       │                                  │
│                               │       │  providers/                      │
│                               │       │  - codex.js                      │
│                               │       │  - kimi-code.js                  │
│                               │       │  - glm-coding.js                 │
└──────────────────────────────┘       └─────────────────────────────────┘
```

关键变化：

- Qt WebEngine **完全移除**。`app/` 目录下的 WebView 窗口和 WebChannel bridge 不再需要。
- DDE Shell 插件增加 WebSocket Server。
- 新增 `extension/` 目录，存放 Chrome Extension 源码。
- `webview` provider 类型重命名为 `browser_ext`。
- `official_api` 和 `manual` provider 保留在 DDE Shell 侧不变。

## 3. 通信协议

### 3.1 连接

- DDE Shell 插件启动 WebSocket Server，监听 `127.0.0.1:18765`（仅 loopback，不暴露到网络）。
- Chrome Extension service worker 启动时主动连接。
- 心跳保活：每 30s 一次 ping/pong。
- 浏览器退出或插件禁用时断开，DDE Shell 侧标记 provider 离线。
- DDE Shell 重启时，Extension 检测断开后自动重连。
- Server 只接受一个连接，后连接的覆盖前一个。

### 3.2 认证

首次配对流程：

1. DDE Shell 插件生成随机 token（32 字节 hex），存入 `~/.config/dde-shell-coding-plan/ext-token`。
2. DDE Shell 插件在设置页面显示 token 供用户复制。
3. 用户在 Chrome Extension popup 中粘贴 token，Extension 通过 `chrome.storage.local` 保存。
4. WS 连接建立后，Extension 发送 `auth` 消息携带 token。
5. DDE Shell 侧验证 token 匹配后才接受后续消息。
6. token 不匹配时 server 关闭连接。

### 3.3 消息格式

所有消息使用 JSON 文本帧。

**认证：**

```json
{
  "type": "auth",
  "token": "<hex-string>"
}
```

**连接状态：**

```json
{
  "type": "status",
  "connected": true,
  "availableProviders": ["codex", "kimi_code", "glm_coding"]
}
```

**刷新请求（DDE Shell → Extension）：**

```json
{
  "type": "refresh_request",
  "requestId": "uuid-1234",
  "providers": ["codex", "kimi_code", "glm_coding"],
  "timeout": 15000
}
```

**刷新结果（Extension → DDE Shell）：**

```json
{
  "type": "refresh_result",
  "requestId": "uuid-1234",
  "provider": "codex",
  "data": {
    "providerId": "codex",
    "providerName": "Codex",
    "source": "browser_ext",
    "status": "ok",
    "remainingRatio": 0.65,
    "used": 350,
    "total": 1000,
    "unit": "credit",
    "balanceText": "350 / 1000 credits",
    "updatedAt": "2026-05-29T10:30:00Z",
    "consoleUrl": "https://chatgpt.com/#settings/usage"
  }
}
```

**打开控制台（DDE Shell → Extension）：**

```json
{
  "type": "open_console",
  "provider": "codex",
  "url": "https://chatgpt.com/#settings/usage"
}
```

**刷新进度（Extension → DDE Shell，可选）：**

```json
{
  "type": "refresh_progress",
  "requestId": "uuid-1234",
  "provider": "kimi_code",
  "status": "loading",
  "message": "正在加载额度页面..."
}
```

## 4. Chrome Extension 设计

### 4.1 目录结构

```
extension/
├── manifest.json
├── service-worker.js
├── offscreen/
│   ├── offscreen.html
│   └── offscreen.js
├── providers/
│   ├── codex.js
│   ├── kimi-code.js
│   └── glm-coding.js
├── shared/
│   ├── quota-types.js
│   └── ws-protocol.js
└── popup/
    ├── popup.html
    └── popup.js
```

### 4.2 manifest.json 关键配置

```json
{
  "manifest_version": 3,
  "name": "DDE Coding Plan Quota Helper",
  "version": "0.1.0",
  "permissions": [
    "offscreen",
    "storage",
    "alarms"
  ],
  "host_permissions": [
    "https://chatgpt.com/*",
    "https://platform.moonshot.cn/*",
    "https://open.bigmodel.cn/*"
  ],
  "background": {
    "service_worker": "service-worker.js"
  },
  "action": {
    "default_popup": "popup/popup.html"
  }
}
```

### 4.3 Service Worker

职责：

- 管理 WebSocket 连接生命周期。
- 收到 `refresh_request` 后串行调度 offscreen document：
  1. `chrome.offscreen.createDocument({url: "offscreen/offscreen.html", reasons: ["IFRAME_SCRIPTING"], justification: "Load quota pages"})`.
  2. 通过 `chrome.runtime.sendMessage` 向 offscreen 发送 provider 配置。
  3. offscreen 完成后回传结果。
  4. `chrome.offscreen.closeDocument()`.
  5. 处理下一个 provider，重复 1-4。
- 每个结果通过 WS 实时发回 DDE Shell。
- 使用 `chrome.alarms` 周期性（每分钟）检查 WS 连接状态，断开时自动重连。

### 4.4 Offscreen Document

职责：

- 收到 provider 配置后，在页面内创建隐藏 iframe 加载 `provider.quotaUrl`。
- 等待页面渲染完成：轮询关键 DOM 元素或使用 MutationObserver，最长等待 10s。
- 调用 `provider.extractQuota(iframe.contentDocument)` 提取数据。
- 调用 `provider.normalizeSnapshot(raw)` 转换为标准格式。
- 通过 `chrome.runtime.sendMessage` 返回结果给 service worker。

**关于 offscreen document 限制：**

Chrome Extension MV3 同一时刻只允许一个 offscreen document。多平台采用串行处理：依次创建 → 加载 → 提取 → 关闭。对于 3-5 个平台、30 分钟刷新一次的场景，串行耗时约 15-30s，完全可接受。

### 4.5 Provider 文件

每个平台一个独立 JS 文件，结构如下（以 codex.js 为例）：

```js
export const codexProvider = {
  id: "codex",
  name: "Codex",
  loginUrl: "https://chatgpt.com/auth/login",
  quotaUrl: "https://chatgpt.com/#settings/usage",
  allowedOrigin: "https://chatgpt.com",
  loginIndicators: [".user-avatar", "[data-testid='profile-button']"],
  extractQuota(doc) {
    const el = doc.querySelector(".usage-value");
    if (!el) return { success: false, reason: "parse_error" };
    return {
      success: true,
      raw: { text: el.textContent, /* ... */ }
    };
  },
  normalizeSnapshot(raw) {
    return {
      providerId: "codex",
      providerName: "Codex",
      source: "browser_ext",
      status: "ok",
      remainingRatio: /* ... */,
      updatedAt: new Date().toISOString()
    };
  }
};
```

页面结构变化时只需更新对应 provider 文件，无需重新编译 C++ 项目。后续新增平台只需添加一个 JS 文件。

### 4.6 Extension Popup（可选）

展示：

- WebSocket 连接状态（已连接 / 未连接）。
- 上次刷新时间和结果摘要。
- 手动触发刷新按钮。
- 打开 DDE Shell 插件设置的链接。

## 5. DDE Shell 侧变更

### 5.1 移除

- `app/` 目录：WebView 窗口、WebChannel bridge。
- CMake 中对 Qt WebEngine 的所有依赖（`Qt6::WebEngineWidgets`、`Qt6::WebChannel`）。
- 现有 `webview` provider 类型的实现代码。
- `web/` 目录中的 React 前端（原用于 WebView 内展示）。

### 5.2 新增

| 文件 | 说明 |
|------|------|
| `src/websocket_server.h/cpp` | WebSocket server，监听 127.0.0.1:18765，处理认证和消息路由 |
| `src/browser_ext_provider.h/cpp` | `browser_ext` provider 类型，替代原 `webview` provider |

### 5.3 browser_ext_provider 工作流程

1. 触发刷新时，通过 WS 发送 `refresh_request`。
2. 等待对应 `requestId` 的 `refresh_result` 回传。
3. 单个 provider 超时 15s，超时未收到结果标记为 `network_error`。
4. WS 未连接（浏览器没开或插件未启用）时，标记为 `unsupported` 并在 popup 中提示用户安装/启动浏览器插件。

### 5.4 新增依赖

- Qt WebSocket 模块（`Qt6::WebSockets`），替代 Qt WebEngine。WebSockets 是轻量模块，几乎所有 Qt 安装都包含。

### 5.5 数据模型变更

- `QuotaSnapshot.source`：`"webview"` → `"browser_ext"`。
- 订阅配置中 `profile_id` 字段改为 `ext_provider_id`。
- 其余字段（`remainingRatio`、`status`、`unit` 等）保持不变。

## 6. 边界情况与降级策略

| 场景 | 处理方式 |
|------|----------|
| 浏览器未运行 | 面板圆环灰色"未连接"；popup 提示启动浏览器；保留上次成功数据（标记 stale） |
| 页面结构变化 | Extension 返回 `{status: "parse_error"}`；DDE Shell 保留上次成功快照不覆盖；Extension popup 展示失败原因 |
| 登录态过期 | Extension 返回 `{status: "auth_error"}`；DDE Shell 展示"请重新登录"按钮，点击后通过 `open_console` 消息让 Extension 打开浏览器到登录页；用户在浏览器中正常登录后，下次刷新自动恢复 |
| DDE Shell 重启 | WS server 重启后监听同一端口；Extension 检测断开后通过 `chrome.alarms` 自动重连 |
| offscreen 超时 | 单个 provider 15s 超时；超时后关闭 offscreen document，跳到下一个 provider；该 provider 返回 `{status: "network_error"}` |
| Service Worker 被 suspend | MV3 下 service worker 空闲 30s 后可能被 suspend；使用 `chrome.alarms`（最小间隔 1 分钟）定期唤醒检查连接；刷新请求期间 service worker 保持活跃不会 suspend |

## 7. 构建与分发

### 7.1 Chrome Extension

- 源码在 `extension/` 目录。
- 开发模式：`chrome://extensions` → 开发者模式 → 加载已解压的扩展程序。
- 分发方式：Chrome Web Store 发布（首选），或提供 `.zip` 包手动安装。
- Extension 独立更新，不需要重新编译 DDE Shell 插件。

### 7.2 DDE Shell 插件

- CMake 构建时不再依赖 Qt WebEngine，改为依赖 Qt WebSockets。
- `web/` 目录和 React 前端构建步骤可以移除。
- `extension/` 目录不参与 CMake 构建，独立管理。

## 8. 迁移路径

从当前架构迁移到新架构的步骤：

1. 新增 `src/websocket_server.h/cpp` 和 `src/browser_ext_provider.h/cpp`。
2. 创建 `extension/` 目录，实现 Chrome Extension 基础框架。
3. 实现三个 provider（codex.js、kimi-code.js、glm-coding.js）的提取逻辑，从现有 C++/JS 提取代码移植。
4. 将 DDE Shell 侧的 `webview` provider 注册替换为 `browser_ext` provider。
5. 移除 `app/` 目录和 Qt WebEngine 依赖。
6. 移除 `web/` 目录和 React 前端。
7. 更新 CMakeLists.txt。
8. 端到端测试：安装 Extension → 配对 → 刷新 → 验证面板展示。

## 9. 保留不变的部分

以下部分不受本次架构变更影响：

- `official_api` provider（MiniMax、Kimi API、OpenAI API）。
- `manual` provider。
- `QuotaSnapshot` 数据结构（仅 `source` 字段值变化）。
- DDE Shell 面板圆环和 Popup UI（`package/main.qml`）。
- 订阅配置管理逻辑。
- 本地存储和安全存储。
- PRD 中定义的产品功能范围和验收标准。
