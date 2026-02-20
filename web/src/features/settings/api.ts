import { call } from '@/core/rpc'
import type { AppSettings } from './types'
import { RESOURCES_DIR, BACKGROUND_IMAGE_NAME } from './constants'

export const settingsApi = {
  get: async (): Promise<AppSettings> => {
    return call<AppSettings>('settings.get')
  },

  patch: async (patch: Partial<AppSettings>): Promise<void> => {
    await call('settings.patch', { patch })
  },
}

/**
 * Get current environment
 */
const getCurrentEnvironment = () => {
  return (window as any).pywebview ? 'webview' : 'web'
}

/**
 * 选择背景图片文件
 */
export async function selectBackgroundImage(): Promise<string | null> {
  try {
    const environment = getCurrentEnvironment()
    const parentWindowMode = environment === 'webview' ? 1 : 2

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
      0
    )

    if (result.paths && result.paths.length > 0) {
      console.log('🖼️ 已选择背景图片:', result.paths[0])
      return result.paths[0] || null
    }

    return null
  } catch (error) {
    console.error('选择背景图片失败:', error)
    throw new Error('选择背景图片失败')
  }
}

/**
 * 复制背景图片到资源目录
 */
export async function copyBackgroundImageToResources(sourcePath: string): Promise<string> {
  try {
    const lastDotIndex = sourcePath.lastIndexOf('.')
    const ext = lastDotIndex !== -1 ? sourcePath.substring(lastDotIndex) : ''
    const destPath = `${RESOURCES_DIR}/${BACKGROUND_IMAGE_NAME}${ext}`

    await call<{
      success: boolean
      message: string
    }>('file.copy', {
      sourcePath: sourcePath,
      destinationPath: destPath,
      overwrite: true,
    })

    console.log('📁 背景图片已复制到资源目录:', destPath)
    return destPath
  } catch (error) {
    console.error('复制背景图片失败:', error)
    throw new Error('复制背景图片失败')
  }
}
