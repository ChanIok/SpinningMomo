import { webviewRPC, type RpcEventHandler } from '@/lib/webview-rpc'
import type { 
  AppSettings, 
  AppInfo, 
  PartialAppSettings,
  GeneralSettings,
  AdvancedSettings
} from '../types'
import { DEFAULT_SETTINGS } from '../types'

// 设置RPC API类，封装所有设置相关的WebView2通信
export class SettingsRPCApi {
  private static instance: SettingsRPCApi

  static getInstance(): SettingsRPCApi {
    if (!this.instance) {
      this.instance = new SettingsRPCApi()
    }
    return this.instance
  }

  // 获取当前设置
  async getSettings(): Promise<AppSettings> {
    try {
      const settings = await webviewRPC.call<AppSettings>('settings.get')
      return this.validateSettings(settings)
    } catch (error) {
      console.error('Failed to get settings:', error)
      throw new Error('获取设置失败')
    }
  }

  // 保存设置（支持部分更新）
  async saveSettings(settings: PartialAppSettings): Promise<void> {
    try {
      // 验证设置数据
      this.validatePartialSettings(settings)
      
      await webviewRPC.call<void>('settings.save', settings)
    } catch (error) {
      console.error('Failed to save settings:', error)
      throw new Error('保存设置失败')
    }
  }

  // 重置设置到默认值
  async resetSettings(): Promise<AppSettings> {
    try {
      const settings = await webviewRPC.call<AppSettings>('settings.reset')
      return this.validateSettings(settings)
    } catch (error) {
      console.error('Failed to reset settings:', error)
      throw new Error('重置设置失败')
    }
  }

  // 获取应用信息
  async getAppInfo(): Promise<AppInfo> {
    try {
      const appInfo = await webviewRPC.call<AppInfo>('settings.getAppInfo')
      return this.validateAppInfo(appInfo)
    } catch (error) {
      console.error('Failed to get app info:', error)
      throw new Error('获取应用信息失败')
    }
  }

  // 监听设置变化事件
  onSettingsChanged(handler: RpcEventHandler<AppSettings>): void {
    webviewRPC.on('settings.changed', (params) => {
      try {
        const settings = this.validateSettings(params as AppSettings)
        handler(settings)
      } catch (error) {
        console.error('Invalid settings in change event:', error)
      }
    })
  }

  // 取消监听设置变化事件
  offSettingsChanged(handler: RpcEventHandler<AppSettings>): void {
    webviewRPC.off('settings.changed', handler as (params: unknown) => void)
  }

  // 验证完整设置对象
  private validateSettings(settings: unknown): AppSettings {
    if (!settings || typeof settings !== 'object') {
      throw new Error('Invalid settings format')
    }

    const s = settings as Record<string, unknown>

    // 验证general设置
    if (!s.general || typeof s.general !== 'object') {
      throw new Error('Invalid general settings')
    }

    // 验证advanced设置
    if (!s.advanced || typeof s.advanced !== 'object') {
      throw new Error('Invalid advanced settings')
    }

    const validatedSettings: AppSettings = {
      general: this.validateGeneralSettings(s.general),
      advanced: this.validateAdvancedSettings(s.advanced)
    }

    return validatedSettings
  }

  // 验证部分设置对象
  private validatePartialSettings(settings: PartialAppSettings): void {
    if (!settings || typeof settings !== 'object') {
      throw new Error('Invalid settings format')
    }

    if (settings.general) {
      this.validateGeneralSettings(settings.general, true)
    }

    if (settings.advanced) {
      this.validateAdvancedSettings(settings.advanced, true)
    }
  }

  // 验证通用设置
  private validateGeneralSettings(general: unknown, partial = false): Partial<GeneralSettings> {
    if (!general || typeof general !== 'object') {
      throw new Error('Invalid general settings')
    }

    const g = general as Record<string, unknown>
    const result: Partial<GeneralSettings> = {}

    // 验证主题
    if ('theme' in g) {
      if (typeof g.theme !== 'string' || !['light', 'dark', 'system'].includes(g.theme)) {
        throw new Error('Invalid theme value')
      }
      result.theme = g.theme as GeneralSettings['theme']
    } else if (!partial) {
      throw new Error('Missing theme in general settings')
    }

    // 验证语言
    if ('language' in g) {
      if (typeof g.language !== 'string' || !['zh', 'en'].includes(g.language)) {
        throw new Error('Invalid language value')
      }
      result.language = g.language as GeneralSettings['language']
    } else if (!partial) {
      throw new Error('Missing language in general settings')
    }

    // 验证布尔值设置
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

  // 验证高级设置
  private validateAdvancedSettings(advanced: unknown, partial = false): Partial<AdvancedSettings> {
    if (!advanced || typeof advanced !== 'object') {
      throw new Error('Invalid advanced settings')
    }

    const a = advanced as Record<string, unknown>
    const result: Partial<AdvancedSettings> = {}

    // 验证日志级别
    if ('logLevel' in a) {
      if (typeof a.logLevel !== 'string' || !['error', 'warn', 'info', 'debug'].includes(a.logLevel)) {
        throw new Error('Invalid logLevel value')
      }
      result.logLevel = a.logLevel as AdvancedSettings['logLevel']
    } else if (!partial) {
      throw new Error('Missing logLevel in advanced settings')
    }

    // 验证最大日志文件数
    if ('maxLogFiles' in a) {
      if (typeof a.maxLogFiles !== 'number' || a.maxLogFiles < 1 || a.maxLogFiles > 100) {
        throw new Error('Invalid maxLogFiles value')
      }
      result.maxLogFiles = a.maxLogFiles
    } else if (!partial) {
      throw new Error('Missing maxLogFiles in advanced settings')
    }

    // 验证布尔值设置
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

  // 验证应用信息
  private validateAppInfo(appInfo: unknown): AppInfo {
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
}

// 导出单例实例
export const settingsRPC = SettingsRPCApi.getInstance()

// 开发环境的模拟API
export const mockSettingsRPC = {
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
      version: '1.0.0',
      buildDate: new Date().toISOString(),
      platform: 'Windows',
      architecture: 'x64'
    }
  },

  onSettingsChanged(): void {
    // Mock实现不需要实际监听
  },

  offSettingsChanged(): void {
    // Mock实现不需要实际取消监听
  }
}

// 根据环境选择API实现
export const settingsAPI = import.meta.env.DEV ? mockSettingsRPC : settingsRPC 