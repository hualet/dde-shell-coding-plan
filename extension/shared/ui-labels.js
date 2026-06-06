export const STATUS_LABELS = {
  disconnected: "未连接",
  connecting: "正在连接...",
  authenticating: "正在认证...",
  connected: "已连接",
  auth_failed: "认证失败",
  error: "连接失败",
  timeout: "连接超时",
  no_token: "未配置 Token",
};

export const STATUS_LABELS_OPTIONS = {
  ...STATUS_LABELS,
  auth_failed: "认证失败（Token 不匹配）",
  no_token: "请输入 Token",
};
