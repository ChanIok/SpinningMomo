import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import type { AppSettings, RuntimeCapabilities } from './types'

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

export interface InfinityNikkiGameDirResult {
  gameDir?: string
  configFound: boolean
  gameDirFound: boolean
  message: string
}

export interface FileInfoResult {
  path: string
  exists: boolean
  isDirectory: boolean
  isRegularFile: boolean
  isSymlink: boolean
  size: number
  extension: string
  filename: string
  lastModified: number
}

export interface BackgroundAnalysisResult {
  themeMode: 'light' | 'dark'
  primaryColor: string
  overlayColors: string[]
  brightness: number
}

export interface BackgroundImportResult {
  imageFileName: string
}

export async function detectInfinityNikkiGameDirectory(): Promise<InfinityNikkiGameDirResult> {
  return call<InfinityNikkiGameDirResult>('extensions.infinityNikki.getGameDirectory', {})
}

export async function selectDirectory(title: string): Promise<string | null> {
  const parentWindowMode = isWebView() ? 1 : 2
  const result = await call<{ path: string }>(
    'dialog.openDirectory',
    {
      title,
      parentWindowMode,
    },
    0
  )

  return result.path || null
}

export async function getFileInfo(path: string): Promise<FileInfoResult> {
  return call<FileInfoResult>('file.getInfo', { path })
}

export async function analyzeBackgroundImage(
  imageFileName: string,
  overlayMode: number
): Promise<BackgroundAnalysisResult> {
  return call<BackgroundAnalysisResult>(
    'settings.background.analyze',
    {
      imageFileName,
      overlayMode,
    },
    0
  )
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
      console.log('已选择背景图片:', result.paths[0])
      return result.paths[0] || null
    }

    return null
  } catch (error) {
    console.error('选择背景图片失败:', error)
    throw new Error('选择背景图片失败')
  }
}

/**
 * 导入背景图片到后端托管目录
 */
export async function importBackgroundImage(sourcePath: string): Promise<string> {
  try {
    const result = await call<BackgroundImportResult>(
      'settings.background.import',
      {
        sourcePath,
      },
      0
    )

    console.log('背景图片已导入到托管目录:', result.imageFileName)
    return result.imageFileName
  } catch (error) {
    console.error('导入背景图片失败:', error)
    throw new Error('导入背景图片失败')
  }
}

/**
 * 删除已管理的背景图片资源（非阻塞容错）
 */
export async function removeBackgroundImageResource(imageFileName: string): Promise<void> {
  try {
    if (!imageFileName) {
      return
    }

    await call('settings.background.remove', {
      imageFileName,
    })
  } catch (error) {
    console.warn('清理旧背景图片失败:', error)
  }
}
