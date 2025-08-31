// 主题设置类型
export interface ThemeSettings {
  mode: 'light' | 'dark' | 'system'
}

// 背景设置类型
export interface WebBackgroundSettings {
  type: 'none' | 'image'
  imagePath: string
  opacity: number
  blurAmount: number
}

// 完整的前端设置类型
export interface WebSettings {
  version: string
  ui: {
    background: WebBackgroundSettings
    theme: ThemeSettings
  }
  createdAt: string
  updatedAt: string
}

// 设置状态类型
export interface WebSettingsState {
  webSettings: WebSettings
  isLoading: boolean
  error: string | null
  isInitialized: boolean
}

// 默认主题设置
export const DEFAULT_THEME_SETTINGS: ThemeSettings = {
  mode: 'system',
} as const

// 默认背景设置
export const DEFAULT_BACKGROUND_SETTINGS: WebBackgroundSettings = {
  type: 'none',
  imagePath: '',
  opacity: 0.8,
  blurAmount: 0,
} as const

// 默认前端设置
export const DEFAULT_WEB_SETTINGS: WebSettings = {
  version: '1.0.0',
  ui: {
    background: DEFAULT_BACKGROUND_SETTINGS,
    theme: DEFAULT_THEME_SETTINGS,
  },
  createdAt: new Date().toISOString(),
  updatedAt: new Date().toISOString(),
} as const
