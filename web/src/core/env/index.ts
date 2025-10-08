/**
 * 运行环境类型
 * - `webview`: 在 WebView2 中运行
 * - `web`: 在标准 Web 浏览器中运行
 */
export type EnvironmentType = 'webview' | 'web'

// 模块级缓存，避免重复检测
let currentEnvironment: EnvironmentType | null = null

/**
 * 检测当前运行环境
 * - `webview`: 通过 `window.chrome.webview` 特征检测
 * - `web`: 默认环境
 */
function detectEnvironment(): EnvironmentType {
  if (typeof window !== 'undefined' && window.chrome?.webview) {
    return 'webview'
  }
  return 'web'
}

/**
 * 获取当前运行环境（带缓存）
 */
export function getCurrentEnvironment(): EnvironmentType {
  if (!currentEnvironment) {
    currentEnvironment = detectEnvironment()
  }
  return currentEnvironment
}

/**
 * 检查是否在 WebView 环境中运行
 */
export function isWebView(): boolean {
  return getCurrentEnvironment() === 'webview'
}

/**
 * 检查是否在标准 Web 浏览器环境中运行
 */
export function isWebBrowser(): boolean {
  return getCurrentEnvironment() === 'web'
}

/**
 * 根据环境获取静态资源的完整 URL
 * - 在 WebView 的生产环境中，使用 `https://static.app.local` 协议头
 * - 在其他环境中，使用相对路径
 */
export function getStaticUrl(path: string): string {
  const normalizedPath = path.startsWith('/') ? path : `/${path}`

  return isWebView() && !import.meta.env.DEV
    ? `https://static.app.local${normalizedPath}`
    : normalizedPath
}
