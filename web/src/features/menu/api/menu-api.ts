import { call } from '@/lib/webview-rpc'
import camelcaseKeys from 'camelcase-keys'
import snakecaseKeys from 'snakecase-keys'
import type { AppSettings } from '../types'

// 服务端数据类型定义（直接对应AppSettings）
interface ServerSettings extends Record<string, unknown> {
  version: string
  title: string
  app_menu: {
    feature_items: unknown[]
    aspect_ratios: unknown[]
    resolutions: unknown[]
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