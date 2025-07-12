import { call } from '@/lib/webview-rpc'
import type { WindowSettings } from '../types'

/**
 * 获取当前窗口设置
 */
export async function getWindowSettings(): Promise<WindowSettings> {
  try {
    const result = await call<{
      window: WindowSettings
      version: string
    }>('settings.get')
    
    return result.window
  } catch (error) {
    console.error('Failed to get window settings:', error)
    throw new Error('获取窗口设置失败')
  }
}

/**
 * 更新窗口设置
 * @param windowSettings 要更新的设置
 */
export async function updateWindowSettings(windowSettings: Partial<WindowSettings>): Promise<void> {
  try {
    await call<{
      success: boolean
      message: string
    }>('settings.update', {
      window: windowSettings
    })
    
    console.log('✅ 窗口设置已更新:', windowSettings)
  } catch (error) {
    console.error('Failed to update window settings:', error)
    throw new Error('更新窗口设置失败')
  }
}

/**
 * 更新窗口标题
 * @param title 新的窗口标题
 */
export async function updateWindowTitle(title: string): Promise<void> {
  return updateWindowSettings({ title })
}