import { call } from '@/lib/webview-rpc'
import type { AppSettings } from './settings-types'

/**
 * 获取当前应用设置
 */
export async function getAppSettings(): Promise<AppSettings> {
  try {
    const result = await call<AppSettings>('settings.get')
    
    console.log('📝 获取应用设置成功:', result)
    
    return result
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
    console.log('📝 更新应用设置:', appSettings)
    
    await call<{
      success: boolean
      message: string
    }>('settings.update', appSettings)
    
    console.log('✅ 应用设置已更新')
  } catch (error) {
    console.error('Failed to update app settings:', error)
    throw new Error('更新应用设置失败')
  }
}