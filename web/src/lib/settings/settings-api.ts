import { call } from '@/lib/webview-rpc'
import camelcaseKeys from 'camelcase-keys'
import snakecaseKeys from 'snakecase-keys'
import type { AppSettings } from './settings-types'

// 服务端数据类型定义（匹配后端重构后的AppSettings结构）
interface ServerSettings extends Record<string, unknown> {
  version: string
  
  // app 分组
  app: {
    hotkey: {
      toggleVisibility: {
        modifiers: number
        key: number
      }
      screenshot: {
        modifiers: number
        key: number
      }
    }
    language: {
      current: string
    }
    logger: {
      level: string
    }
  }
  
  // window 分组
  window: {
    target_title: string
    taskbar: {
      auto_hide: boolean
      lower_on_resize: boolean
    }
  }
  
  // features 分组
  features: {
    screenshot: {
      game_album_path: string
    }
    letterbox: {
      enabled: boolean
    }
  }
  
  // ui 分组
  ui: {
    app_menu: {
      feature_items: unknown[]
      aspect_ratios: unknown[]
      resolutions: unknown[]
    }
    app_window_layout: {
      base_item_height: number
      base_title_height: number
      base_separator_height: number
      base_font_size: number
      base_text_padding: number
      base_indicator_width: number
      base_ratio_indicator_width: number
      base_ratio_column_width: number
      base_resolution_column_width: number
      base_settings_column_width: number
    }
  }
}

/**
 * 获取当前应用设置
 */
export async function getAppSettings(): Promise<AppSettings> {
  try {
    const result = await call<ServerSettings>('settings.get')
    
    // 将服务端的下划线命名转换为驼峰命名
    const camelCaseSettings = camelcaseKeys(result as ServerSettings, { deep: true }) as AppSettings
    
    console.log('🔄 原始数据:', result)
    console.log('🔄 转换后数据:', camelCaseSettings)
    
    return camelCaseSettings
  } catch (error) {
    console.error('Failed to get app settings:', error)
    throw new Error('获取应用设置失败')
  }
}

/**
 * 更新应用设置
 * @param appSettings 要更新的设置
 */
export async function updateAppSettings(appSettings: AppSettings): Promise<void> {
  try {
    // 将前端的驼峰命名转换为服务端的下划线命名
    // 使用 unknown 进行安全的类型转换
    const snakeCaseSettings = snakecaseKeys(appSettings as unknown as Record<string, unknown>, { deep: true })
    
    console.log('🔄 前端数据:', appSettings)
    console.log('🔄 发送数据:', snakeCaseSettings)
    
    await call<{
      success: boolean
      message: string
    }>('settings.update', snakeCaseSettings)
    
    console.log('✅ 应用设置已更新')
  } catch (error) {
    console.error('Failed to update app settings:', error)
    throw new Error('更新应用设置失败')
  }
}