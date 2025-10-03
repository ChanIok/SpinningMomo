// 环境检测模块
export type EnvironmentType = 'webview' | 'web'

// 检测运行环境
export function detectEnvironment(): EnvironmentType {
  if (typeof window !== 'undefined' && window.chrome?.webview) {
    return 'webview'
  }
  return 'web'
}

// 检查是否在 WebView 环境中运行
export function isWebView(): boolean {
  return detectEnvironment() === 'webview'
}

// 检查是否在 Web 浏览器环境中运行
export function isWebBrowser(): boolean {
  return detectEnvironment() === 'web'
}

// 模块级缓存
let currentEnvironment: EnvironmentType | null = null

// 获取当前运行环境（带缓存）
export function getCurrentEnvironment(): EnvironmentType {
  if (!currentEnvironment) {
    currentEnvironment = detectEnvironment()
  }
  return currentEnvironment
}

// 获取静态资源的完整 URL
export function getStaticUrl(path: string): string {
  const normalizedPath = path.startsWith('/') ? path : `/${path}`

  return getCurrentEnvironment() === 'webview' && !import.meta.env.DEV
    ? `https://static.app.local${normalizedPath}`
    : normalizedPath
}
