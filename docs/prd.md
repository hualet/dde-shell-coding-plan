# dde-shell Coding Plan 额度插件 PRD

更新日期：2026-05-25

## 1. 背景

开发者同时订阅多个 AI Coding Plan 后，额度分散在不同控制台、Web 页面或客户端中，日常很难快速判断哪个服务还可用、哪个服务即将耗尽。本项目希望在 DDE Shell 面板中提供一个轻量插件，集中显示 Coding Plan 额度剩余情况，降低切换平台和手动查询成本。

本 PRD 基于当前调研和产品决策制定：首版统一采用内置 WebView + WebChannel 作为 Coding Plan 类订阅的主要接入方式，让用户在官方网页内完成登录，由插件在受控会话中读取额度页面并归一化展示。第一阶段优先适配 Codex、Kimi Code、GLM Coding。

## 2. 产品目标

1. 在 DDE Shell 面板中一眼看到已添加 Coding Plan 的剩余额度状态。
2. 点击插件后在弹窗中查看各平台额度详情、刷新时间和异常状态。
3. 用统一 WebView 登录与页面读取能力覆盖没有稳定公开 quota API 的 Coding Plan。
4. 支持供应商扩展，避免后续新增平台时重写 UI 与数据模型。
5. 对页面结构变更、登录失效、读取失败等情况提供明确降级，保证 MVP 可交付。

## 3. 用户与场景

目标用户是使用 deepin/DDE 桌面环境，并同时使用多个 AI 编程服务的开发者。

核心场景：

- 开发前快速判断当前应该使用哪个 Coding Plan。
- 额度接近耗尽时及时切换供应商或充值。
- 第一次添加平台时，在插件内打开官方网页完成登录。
- 某个平台页面读取失败时，仍能打开官方控制台人工查看或临时手动维护额度。

## 4. 范围

### 4.1 MVP 范围

- DDE Shell 面板插件入口。
- 面板上显示多个类似 iOS 电池组件的圆环，每个圆环代表一个已启用平台的额度剩余比例。
- 点击后展示 popup，包含全部已添加订阅的额度详情。
- 支持添加、编辑、删除订阅配置。
- 内置 WebView 登录容器和平台会话管理。
- 通过 WebChannel 执行受控页面读取，并返回标准化额度快照。
- 第一阶段支持 Codex、Kimi Code、GLM Coding 三个 WebView provider。
- 保留 API provider 能力，用于 MiniMax Token Plan、Kimi API、OpenAI API 等已有官方接口的平台。
- 支持失败状态、上次成功刷新时间、手动刷新、打开控制台。

### 4.2 非目标

- 不在首版实现购买、续费、支付或自动充值。
- 不绕过平台登录验证，不伪装官方客户端身份。
- 不导出、不上传、不跨平台复用用户 Cookie。
- 不承诺对未公开页面结构的读取长期稳定。
- 不做跨设备云同步。

## 5. 平台支持策略

| 平台 | 首版能力 | 数据来源 | 备注 |
| --- | --- | --- | --- |
| Codex / ChatGPT | WebView 登录 + 页面读取 | 官方 Codex/ChatGPT usage 页面 | 第一阶段重点；无稳定公开 quota API，读取失败时打开官方页面 |
| Kimi Code | WebView 登录 + 页面读取 | Kimi Code Console | 第一阶段重点；不混用 Kimi API 余额 |
| GLM / 智谱 Coding | WebView 登录 + 页面读取 | 智谱开放平台 / Coding 控制台 | 第一阶段重点；保留 API Key 鉴权检查作为辅助 |
| MiniMax Token Plan | 官方 API 自动查询 | Token Plan API / CLI | 已有官方 quota 路径，优先 official_api provider |
| Kimi API / Moonshot API | 官方 API 自动查询 | `GET /v1/users/me/balance` | API 余额，不等同 Kimi Code 会员额度 |
| OpenAI API | 官方 API 自动查询 | Usage/Costs API | API 账单，不等同 ChatGPT/Codex 订阅 |

## 6. 总体方案

首版采用两类 provider：

1. `webview` provider：面向 Codex、Kimi Code、GLM Coding 等没有稳定公开 quota API 的 Coding Plan。插件提供独立 WebView profile，用户在官方页面登录；provider 进入额度页面后，通过受控脚本读取 DOM 或页面内公开 JSON，再经 WebChannel 回传标准化结果。
2. `official_api` provider：面向 MiniMax Token Plan、Kimi API、OpenAI API 等已有官方接口的平台。用户配置必要凭证后，provider 直接请求官方 API。

UI 和本地数据层只消费统一的 `QuotaSnapshot`，不关心数据来自 WebView、官方 API 还是手动录入。这样后续某个平台发布官方 quota API 时，可以只替换 provider 内部实现，不影响面板和 popup。

## 7. 功能需求

### 7.1 面板概览

- 默认展示 1 到 4 个圆环，优先显示用户置顶或额度最低的平台。
- 圆环展示剩余比例、平台标识和状态颜色。
- 状态颜色建议：
  - 正常：剩余比例大于 30%。
  - 预警：剩余比例 10% 到 30%。
  - 危急：剩余比例低于 10%。
  - 异常：最近一次刷新失败、登录失效或读取失败。
- 无配置时展示空状态入口，引导添加第一个订阅。

### 7.2 弹窗详情

- 展示所有订阅卡片，包括平台、计划名称、剩余量、总量、周期、剩余比例、上次刷新时间。
- 展示数据来源：WebView、官方 API、手动录入。
- 支持单个平台手动刷新。
- 支持打开平台官网、额度页或控制台。
- WebView 读取失败时展示错误摘要和降级建议。
- 手动模式下支持快速编辑剩余量和总量。

### 7.3 订阅管理

- 添加订阅时选择平台、数据来源模式、显示名称、计划周期、总额度。
- WebView 模式需要展示登录状态、登录入口、清理会话入口。
- API 模式需要填写必要凭证，例如 API Key、组织 ID 或 base URL。
- 手动模式只需要填写总额度、剩余额度、周期起止时间。
- 支持设置置顶、隐藏、删除。
- 支持为每个平台配置或覆盖控制台 URL。

### 7.4 WebView 登录与读取

- 每个平台使用独立 WebView profile，避免不同供应商会话混用。
- 用户在官方网页中完成登录，插件不接管用户名、密码、验证码或二次验证。
- provider 定义 `loginUrl`、`quotaUrl`、`isLoggedIn()`、`extractQuota()`、`normalizeSnapshot()`。
- WebChannel 只暴露最小能力：
  - 回传额度读取结果。
  - 回传登录状态。
  - 打开外部链接。
  - 清理当前 provider 会话。
- 页面读取脚本必须按 provider 白名单注入，只在已声明的官方域名运行。
- 不允许网页调用任意本地命令、读取本地文件或访问其他 provider 会话。

### 7.5 数据刷新

- 默认每 30 分钟后台刷新一次已启用 provider。
- WebView provider 可以在后台创建隐藏页面刷新，但必须有超时和退避策略。
- 支持用户手动刷新全部或单个平台。
- 刷新失败不覆盖上一次成功数据，记录错误状态与失败时间。
- 对页面结构变更、未登录、网络错误、限流、认证失败进行分类提示。

### 7.6 安全与隐私

- API Key 等敏感信息必须存储在本地安全存储中，不能明文写入普通配置文件。
- WebView Cookie 和 localStorage 仅保存在本机对应 provider profile 中，不上传、不导出。
- 日志中不得输出完整凭证、Cookie、授权头或页面敏感内容。
- WebChannel 不提供通用本地执行接口。
- 删除订阅时同步删除对应凭证和 provider WebView profile。

## 8. 数据模型

订阅配置：

- `id`：本地唯一 ID。
- `provider`：供应商类型。
- `display_name`：展示名称。
- `source_type`：`webview`、`official_api`、`manual`、`console_link`。
- `plan_name`：计划名称。
- `period_start` / `period_end`：额度周期。
- `total_quota`：总额度。
- `remaining_quota`：剩余额度。
- `unit`：额度单位，例如 tokens、credits、requests、CNY、percent。
- `pinned`：是否优先展示。
- `console_url`：控制台地址。
- `profile_id`：WebView provider 对应的本地会话 profile。
- `last_success_at`：上次成功刷新时间。
- `last_error`：最近错误摘要。

标准化额度快照：

```ts
type QuotaSnapshot = {
  providerId: string;
  providerName: string;
  source: "webview" | "official_api" | "manual" | "console_link";
  status:
    | "ok"
    | "warning"
    | "exhausted"
    | "auth_error"
    | "rate_limited"
    | "unsupported"
    | "parse_error"
    | "network_error";
  remainingRatio?: number;
  used?: number;
  total?: number;
  unit?: "request" | "token" | "credit" | "currency" | "percent";
  balanceText?: string;
  resetAt?: string;
  updatedAt: string;
  consoleUrl?: string;
  message?: string;
};
```

供应商适配器：

- `provider`：供应商标识。
- `sourceType`：数据来源类型。
- `capability`：支持自动查询、WebView 登录、手动录入、控制台跳转的能力集合。
- `validateConfig(config)`：验证配置完整性。
- `fetchQuota(config)`：返回标准化额度结果。

WebView provider 额外接口：

- `loginUrl`：登录入口。
- `quotaUrl`：额度页面入口。
- `allowedOrigins`：允许注入脚本和 WebChannel 通信的域名。
- `isLoggedIn(page)`：判断当前会话是否已登录。
- `extractQuota(page)`：读取页面额度信息。
- `normalizeSnapshot(raw)`：转换为 `QuotaSnapshot`。

## 9. 交互要求

- 面板圆环需要保持紧凑，避免占用过多任务栏空间。
- popup 中的信息密度优先于装饰性设计，适合频繁查看。
- WebView 登录页作为配置流程的一部分出现，不抢占日常查看路径。
- 异常状态要能被快速识别，但不应频繁打扰用户。
- 对 WebView 读取结果标记来源和更新时间，避免用户误以为它是平台官方 API 数据。
- 手动录入入口要足够明显，避免页面读取失败时用户无路可走。

## 10. 验收标准

1. 用户可以添加至少 Codex、Kimi Code、GLM Coding 三种 WebView provider。
2. 用户可以在插件内打开官方登录页并完成登录，插件能记录每个 provider 的本地会话状态。
3. Codex、Kimi Code、GLM Coding 至少能在登录后进入对应额度页，并成功读取或明确提示不可读取原因。
4. WebView 读取失败时保留上次成功数据，并提供打开官方控制台的入口。
5. MiniMax、Kimi API、OpenAI API 可以作为 official_api provider 接入或在文档中保留后续实现路径。
6. 面板圆环能够正确反映正常、预警、危急、异常四类状态。
7. 删除订阅后，本地配置、敏感凭证和对应 WebView profile 同步清理。
8. README 能链接到本 PRD，后续开发者可以按文档拆分实现任务。

## 11. 里程碑

### M1：插件骨架与统一模型

- 建立 DDE Shell 插件基础结构。
- 实现面板圆环和 popup 静态数据展示。
- 定义订阅配置、`QuotaSnapshot`、provider 接口。
- 实现本地订阅配置读写。

### M2：WebView 基础能力

- 接入内置 WebView 与独立 provider profile。
- 实现登录页、额度页、会话状态、清理会话。
- 实现最小 WebChannel 能力和域名白名单。
- 实现 WebView provider 的刷新、超时和错误分类。

### M3：首批 Coding Plan Provider

- 实现 Codex / ChatGPT WebView provider。
- 实现 Kimi Code WebView provider。
- 实现 GLM Coding WebView provider。
- 为三家 provider 提供打开官方控制台和手动录入降级。

### M4：官方 API Provider 与安全收口

- 接入本地安全存储。
- 实现 MiniMax Token Plan official_api provider。
- 实现 Kimi API balance 或 OpenAI API usage provider 中至少一个。
- 完善日志脱敏、删除清理、核心单元测试和插件手工验收清单。

## 12. 风险与对策

- 页面结构变更：每个 WebView provider 独立 extractor，失败不影响其他 provider，并保留上次成功数据。
- 平台登录风控：只在官方页面内登录，不伪装客户端，不接管密码或验证码。
- WebChannel 权限过大：采用 provider 域名白名单和最小接口，不暴露任意本地执行能力。
- 平台额度单位不统一：统一为“显示单位 + 原始数值”，不强行跨平台换算。
- 敏感凭证泄露风险：使用系统安全存储，WebView 会话本地隔离，日志脱敏，删除订阅时清理凭证和 profile。
- DDE Shell 插件 UI 空间有限：面板只展示摘要，细节放入 popup。

## 13. 待确认事项

- Codex、Kimi Code、GLM Coding 各自最适合读取的额度页 URL 和页面字段需要用真实账号验证。
- WebView 是否允许后台隐藏刷新，还是必须在用户打开 popup 时刷新。
- GLM Coding 的 Web 控制台是否有稳定的套餐剩余展示字段。
- official_api provider 在首版是否必须包含 MiniMax，还是放到 WebView MVP 后实现。
