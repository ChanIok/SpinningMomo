import { call } from '@/lib/rpc'
import type { WebSettings } from './webSettingsTypes'
import { DEFAULT_WEB_SETTINGS } from './webSettingsTypes'
import { CONFIG_PATH, IMAGE_FILE_FILTER } from './constants'
import { getCurrentEnvironment } from '@/lib/environment'

/**
 * 读取前端配置文件
 */
export async function readWebSettings(): Promise<WebSettings | null> {
  try {
    const result = await call<{
      content: string
      exists: boolean
    }>('file.read', { path: CONFIG_PATH })

    if (!result.exists) {
      console.log('📁 前端配置文件不存在')
      return null
    }

    const settings = JSON.parse(result.content) as WebSettings
    console.log('📁 读取前端配置成功:', settings)
    return settings
  } catch (error) {
    console.error('读取前端配置失败:', error)
    throw new Error('读取前端配置失败')
  }
}

/**
 * 写入前端配置文件
 */
export async function writeWebSettings(settings: WebSettings): Promise<void> {
  try {
    const updatedSettings = {
      ...settings,
      updatedAt: new Date().toISOString(),
    }

    await call<{
      success: boolean
      message: string
    }>('file.write', {
      path: CONFIG_PATH,
      content: JSON.stringify(updatedSettings, null, 2),
      isBinary: false,
      overwrite: true,
    })

    console.log('📁 写入前端配置成功:', updatedSettings)
  } catch (error) {
    console.error('写入前端配置失败:', error)
    throw new Error('写入前端配置失败')
  }
}

/**
 * 检查前端配置文件是否存在
 */
export async function checkWebSettingsExists(): Promise<boolean> {
  try {
    const result = await call<{
      exists: boolean
      is_file: boolean
    }>('file.getInfo', { path: CONFIG_PATH })

    return result.exists && result.is_file
  } catch (error) {
    console.error('检查前端配置文件失败:', error)
    return false
  }
}

/**
 * 初始化前端配置文件
 */
export async function initializeWebSettings(): Promise<WebSettings> {
  try {
    console.log('📁 初始化前端配置文件...')

    const exists = await checkWebSettingsExists()

    if (exists) {
      const settings = await readWebSettings()
      if (settings) {
        console.log('✅ 前端配置文件已存在，使用现有配置')
        return settings
      }
    }

    // 创建默认配置
    const defaultSettings = {
      ...DEFAULT_WEB_SETTINGS,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    }

    await writeWebSettings(defaultSettings)
    console.log('✅ 前端配置文件初始化完成')
    return defaultSettings
  } catch (error) {
    console.error('初始化前端配置失败:', error)
    throw new Error('初始化前端配置失败')
  }
}

/**
 * 选择背景图片文件
 */
export async function selectBackgroundImage(): Promise<string | null> {
  try {
    // 根据当前环境选择父窗口模式
    const environment = getCurrentEnvironment()
    const parentWindowMode = environment === 'webview' ? 1 : 2 // webview: 1, web: 2

    const result = await call<{
      paths: string[]
    }>(
      'dialog.openFile',
      {
        title: '选择背景图片',
        filter:
          '图片文件 (*.jpg;*.jpeg;*.png;*.bmp;*.gif)|*.jpg;*.jpeg;*.png;*.bmp;*.gif|所有文件 (*.*)|*.*',
        allow_multiple: false,
        parentWindowMode,
      },
      -1
    ) // 永不超时

    if (result.paths && result.paths.length > 0) {
      console.log('🖼️ 已选择背景图片:', result.paths[0])
      return result.paths[0]
    }

    return null
  } catch (error) {
    console.error('选择背景图片失败:', error)
    throw new Error('选择背景图片失败')
  }
}
