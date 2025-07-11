// 设置页面类型
export type SettingsPage = 'about' | 'general' | 'advanced'

// 设置侧边栏项目
export interface SettingsSidebarItem {
  id: SettingsPage
  label: string
  icon: React.ComponentType<{ className?: string }>
  description?: string
}

// 通用设置类型
export interface GeneralSettings {
  theme: 'light' | 'dark' | 'system'
  language: 'zh' | 'en'
  autoStart: boolean
  minimizeToTray: boolean
  notifications: boolean
}

// 高级设置类型
export interface AdvancedSettings {
  debugMode: boolean
  logLevel: 'error' | 'warn' | 'info' | 'debug'
  maxLogFiles: number
  performanceMode: boolean
  experimentalFeatures: boolean
}

// 应用信息类型
export interface AppInfo {
  version: string
  buildDate: string
  platform: string
  architecture: string
}

// 完整设置接口
export interface AppSettings {
  general: GeneralSettings
  advanced: AdvancedSettings
}

// RPC方法类型定义
export interface SettingsRPCGetSettings {
  method: 'settings.get'
  params: void
  result: AppSettings
}

export interface SettingsRPCSaveSettings {
  method: 'settings.save'
  params: Partial<AppSettings>
  result: void
}

export interface SettingsRPCResetSettings {
  method: 'settings.reset'
  params: void
  result: AppSettings
}

export interface SettingsRPCGetAppInfo {
  method: 'settings.getAppInfo'
  params: void
  result: AppInfo
}

export interface SettingsRPCSettingsChanged {
  method: 'settings.changed'
  params: AppSettings
}

export type SettingsRPCMethods = SettingsRPCGetSettings | SettingsRPCSaveSettings | SettingsRPCResetSettings | SettingsRPCGetAppInfo
export type SettingsRPCEvents = SettingsRPCSettingsChanged

// 设置验证规则
export interface SettingsValidationRules {
  general: {
    theme: Array<GeneralSettings['theme']>
    language: Array<GeneralSettings['language']>
  }
  advanced: {
    logLevel: Array<AdvancedSettings['logLevel']>
    maxLogFiles: {
      min: number
      max: number
    }
  }
}

// 默认设置值
export const DEFAULT_SETTINGS: AppSettings = {
  general: {
    theme: 'system',
    language: 'zh',
    autoStart: false,
    minimizeToTray: true,
    notifications: true,
  },
  advanced: {
    debugMode: false,
    logLevel: 'info',
    maxLogFiles: 10,
    performanceMode: false,
    experimentalFeatures: false,
  }
} as const

// 设置验证规则常量
export const SETTINGS_VALIDATION_RULES: SettingsValidationRules = {
  general: {
    theme: ['light', 'dark', 'system'],
    language: ['zh', 'en'],
  },
  advanced: {
    logLevel: ['error', 'warn', 'info', 'debug'],
    maxLogFiles: {
      min: 1,
      max: 100,
    },
  }
} as const

// 类型守卫函数
export function isValidTheme(value: unknown): value is GeneralSettings['theme'] {
  return typeof value === 'string' && SETTINGS_VALIDATION_RULES.general.theme.includes(value as GeneralSettings['theme'])
}

export function isValidLanguage(value: unknown): value is GeneralSettings['language'] {
  return typeof value === 'string' && SETTINGS_VALIDATION_RULES.general.language.includes(value as GeneralSettings['language'])
}

export function isValidLogLevel(value: unknown): value is AdvancedSettings['logLevel'] {
  return typeof value === 'string' && SETTINGS_VALIDATION_RULES.advanced.logLevel.includes(value as AdvancedSettings['logLevel'])
}

// 设置部分更新类型
export type PartialAppSettings = {
  [K in keyof AppSettings]?: Partial<AppSettings[K]>
}

// 设置键路径类型
export type SettingsKeyPath = 
  | `general.${keyof GeneralSettings}`
  | `advanced.${keyof AdvancedSettings}`

// 向后兼容（已废弃，使用JSON-RPC替代）
/** @deprecated 使用 JSON-RPC 替代 */
export interface WebViewMessage<T = unknown> {
  type: string
  data?: T
  requestId?: string
}

/** @deprecated 使用 JSON-RPC 替代 */
export interface ApiResponse<T = unknown> {
  success: boolean
  data?: T
  error?: string
} 