import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import type { AppSettings, RuntimeCapabilities } from './types'
import { BACKGROUND_IMAGE_NAME, BACKGROUND_RESOURCES_DIR, BACKGROUND_WEB_DIR } from './constants'
import { isManagedBackgroundPath, toResourceFilePath } from './backgroundPath'

export const settingsApi = {
  get: async (): Promise<AppSettings> => {
    return call<AppSettings>('settings.get')
  },

  getRuntimeCapabilities: async (): Promise<RuntimeCapabilities> => {
    return call<RuntimeCapabilities>('runtime_info.get')
  },

  patch: async (patch: Partial<AppSettings>): Promise<void> => {
    await call('settings.patch', { patch })
  },
}

/**
 * 选择背景图片文件
 */
export async function selectBackgroundImage(): Promise<string | null> {
  try {
    const parentWindowMode = isWebView() ? 1 : 2

    const result = await call<{
      paths: string[]
    }>(
      'dialog.openFile',
      {
        title: '选择背景图片',
        filter:
          '图片文件 (*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp)|*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp|所有文件 (*.*)|*.*',
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
    const ext = lastDotIndex !== -1 ? sourcePath.substring(lastDotIndex).toLowerCase() : ''
    const safeExt = ext.match(/^\.(jpg|jpeg|png|bmp|gif|webp)$/i) ? ext : '.jpg'
    const revision = `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 8)}`
    const fileName = `${BACKGROUND_IMAGE_NAME}-${revision}${safeExt}`
    const destPath = `${BACKGROUND_RESOURCES_DIR}/${fileName}`

    await call<{
      success: boolean
      message: string
    }>('file.copy', {
      sourcePath: sourcePath,
      destinationPath: destPath,
      overwrite: true,
    })

    console.log('📁 背景图片已复制到资源目录:', destPath)
    return `${BACKGROUND_WEB_DIR}/${fileName}`
  } catch (error) {
    console.error('复制背景图片失败:', error)
    throw new Error('复制背景图片失败')
  }
}

/**
 * 删除已管理的背景图片资源（非阻塞容错）
 */
export async function removeBackgroundImageResource(imagePath: string): Promise<void> {
  try {
    if (!imagePath || !isManagedBackgroundPath(imagePath)) {
      return
    }

    const resourcePath = toResourceFilePath(imagePath)
    if (!resourcePath) {
      return
    }

    await call('file.delete', {
      path: resourcePath,
      recursive: false,
    })
  } catch (error) {
    console.warn('清理旧背景图片失败:', error)
  }
}
