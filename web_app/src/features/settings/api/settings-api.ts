import { call, on, off, type RpcEventHandler } from '@/lib/webview-rpc'
import type {
  AppSettings,
  AppInfo,
  PartialAppSettings,
  GeneralSettings,
  AdvancedSettings
} from '../types'
import { DEFAULT_SETTINGS } from '../types'

// --- 内部验证函数 ---

function validateGeneralSettings(general: unknown, partial = false): Partial<GeneralSettings> {
  if (!general || typeof general !== 'object') {
    throw new Error('Invalid general settings')
  }

  const g = general as Record<string, unknown>
  const result: Partial<GeneralSettings> = {}

  if ('theme' in g) {
    if (typeof g.theme !== 'string' || !['light', 'dark', 'system'].includes(g.theme)) {
      throw new Error('Invalid theme value')
    }
    result.theme = g.theme as GeneralSettings['theme']
  } else if (!partial) {
    throw new Error('Missing theme in general settings')
  }

  if ('language' in g) {
    if (typeof g.language !== 'string' || !['zh', 'en'].includes(g.language)) {
      throw new Error('Invalid language value')
    }
    result.language = g.language as GeneralSettings['language']
  } else if (!partial) {
    throw new Error('Missing language in general settings')
  }

  const booleanFields = ['autoStart', 'minimizeToTray', 'notifications'] as const
  for (const field of booleanFields) {
    if (field in g) {
      if (typeof g[field] !== 'boolean') {
        throw new Error(`Invalid ${field} value`)
      }
      result[field] = g[field] as boolean
    } else if (!partial) {
      throw new Error(`Missing ${field} in general settings`)
    }
  }

  return result
}

function validateAdvancedSettings(advanced: unknown, partial = false): Partial<AdvancedSettings> {
  if (!advanced || typeof advanced !== 'object') {
    throw new Error('Invalid advanced settings')
  }

  const a = advanced as Record<string, unknown>
  const result: Partial<AdvancedSettings> = {}

  if ('logLevel' in a) {
    if (typeof a.logLevel !== 'string' || !['error', 'warn', 'info', 'debug'].includes(a.logLevel)) {
      throw new Error('Invalid logLevel value')
    }
    result.logLevel = a.logLevel as AdvancedSettings['logLevel']
  } else if (!partial) {
    throw new Error('Missing logLevel in advanced settings')
  }

  if ('maxLogFiles' in a) {
    if (typeof a.maxLogFiles !== 'number' || a.maxLogFiles < 1 || a.maxLogFiles > 100) {
      throw new Error('Invalid maxLogFiles value')
    }
    result.maxLogFiles = a.maxLogFiles
  } else if (!partial) {
    throw new Error('Missing maxLogFiles in advanced settings')
  }

  const booleanFields = ['debugMode', 'performanceMode', 'experimentalFeatures'] as const
  for (const field of booleanFields) {
    if (field in a) {
      if (typeof a[field] !== 'boolean') {
        throw new Error(`Invalid ${field} value`)
      }
      result[field] = a[field] as boolean
    } else if (!partial) {
      throw new Error(`Missing ${field} in advanced settings`)
    }
  }

  return result
}

function validateSettings(settings: unknown): AppSettings {
  if (!settings || typeof settings !== 'object') {
    throw new Error('Invalid settings format')
  }
  const s = settings as Record<string, unknown>
  if (!s.general || typeof s.general !== 'object') {
    throw new Error('Invalid general settings')
  }
  if (!s.advanced || typeof s.advanced !== 'object') {
    throw new Error('Invalid advanced settings')
  }
  return {
    general: validateGeneralSettings(s.general) as GeneralSettings,
    advanced: validateAdvancedSettings(s.advanced) as AdvancedSettings
  }
}

function validatePartialSettings(settings: PartialAppSettings): void {
  if (!settings || typeof settings !== 'object') {
    throw new Error('Invalid settings format')
  }
  if (settings.general) {
    validateGeneralSettings(settings.general, true)
  }
  if (settings.advanced) {
    validateAdvancedSettings(settings.advanced, true)
  }
}

function validateAppInfo(appInfo: unknown): AppInfo {
  if (!appInfo || typeof appInfo !== 'object') {
    throw new Error('Invalid app info format')
  }
  const info = appInfo as Record<string, unknown>
  const requiredFields = ['version', 'buildDate', 'platform', 'architecture']
  for (const field of requiredFields) {
    if (!(field in info) || typeof info[field] !== 'string') {
      throw new Error(`Invalid or missing ${field} in app info`)
    }
  }
  return {
    version: info.version as string,
    buildDate: info.buildDate as string,
    platform: info.platform as string,
    architecture: info.architecture as string
  }
}

// --- 真实 RPC API 实现 ---

async function getSettings(): Promise<AppSettings> {
  try {
    const settings = await call<AppSettings>('settings.get')
    return validateSettings(settings)
  } catch (error) {
    console.error('Failed to get settings:', error)
    throw new Error('获取设置失败')
  }
}

async function saveSettings(settings: PartialAppSettings): Promise<void> {
  try {
    validatePartialSettings(settings)
    await call<void>('settings.save', settings)
  } catch (error) {
    console.error('Failed to save settings:', error)
    throw new Error('保存设置失败')
  }
}

async function resetSettings(): Promise<AppSettings> {
  try {
    const settings = await call<AppSettings>('settings.reset')
    return validateSettings(settings)
  } catch (error) {
    console.error('Failed to reset settings:', error)
    throw new Error('重置设置失败')
  }
}

async function getAppInfo(): Promise<AppInfo> {
  try {
    const appInfo = await call<AppInfo>('settings.getAppInfo')
    return validateAppInfo(appInfo)
  } catch (error) {
    console.error('Failed to get app info:', error)
    throw new Error('获取应用信息失败')
  }
}

function onSettingsChanged(handler: RpcEventHandler<AppSettings>): void {
  on('settings.changed', (params) => {
    try {
      const settings = validateSettings(params as AppSettings)
      handler(settings)
    } catch (error) {
      console.error('Invalid settings in change event:', error)
    }
  })
}

function offSettingsChanged(handler: RpcEventHandler<AppSettings>): void {
  off('settings.changed', handler as (params: unknown) => void)
}

const rpcAPI = {
  getSettings,
  saveSettings,
  resetSettings,
  getAppInfo,
  onSettingsChanged,
  offSettingsChanged,
}

// --- 开发环境的模拟 API ---

const mockAPI = {
  async getSettings(): Promise<AppSettings> {
    await new Promise(resolve => setTimeout(resolve, 300))
    return structuredClone(DEFAULT_SETTINGS)
  },
  async saveSettings(settings: PartialAppSettings): Promise<void> {
    await new Promise(resolve => setTimeout(resolve, 200))
    console.log('Mock: Saving settings', settings)
  },
  async resetSettings(): Promise<AppSettings> {
    await new Promise(resolve => setTimeout(resolve, 300))
    return structuredClone(DEFAULT_SETTINGS)
  },
  async getAppInfo(): Promise<AppInfo> {
    await new Promise(resolve => setTimeout(resolve, 100))
    return {
      version: '1.0.0-mock',
      buildDate: new Date().toISOString(),
      platform: 'Windows',
      architecture: 'x64'
    }
  },
  onSettingsChanged(): void {
    // Mock 实现不需要实际监听
  },
  offSettingsChanged(): void {
    // Mock 实现不需要实际取消监听
  }
}

// --- 根据环境选择 API 实现 ---
export const settingsAPI = import.meta.env.DEV ? mockAPI : rpcAPI