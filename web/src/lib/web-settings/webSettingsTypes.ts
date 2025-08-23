// 背景设置类型
export interface WebBackgroundSettings {
  type: 'none' | 'image'
  imagePath: string
  opacity: number
}

// 完整的前端设置类型
export interface WebSettings {
  version: string
  ui: {
    background: WebBackgroundSettings
  }
  createdAt: string
  updatedAt: string
}

// 设置状态类型
export interface WebSettingsState {
  settings: WebSettings
  isLoading: boolean
  error: string | null
  isInitialized: boolean
}

// 默认背景设置
export const DEFAULT_BACKGROUND_SETTINGS: WebBackgroundSettings = {
  type: 'none',
  imagePath: '',
  opacity: 1.0
} as const

// 默认前端设置
export const DEFAULT_WEB_SETTINGS: WebSettings = {
  version: '1.0.0',
  ui: {
    background: DEFAULT_BACKGROUND_SETTINGS
  },
  createdAt: new Date().toISOString(),
  updatedAt: new Date().toISOString()
} as const
