import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { isLocalAccess } from '@/core/access'
import type { AppSettings, RuntimeCapabilities } from './types'

// 设置页 RPC 入口；LAN 状态和令牌接口只暴露给本机设置页面。
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

  getLanAccessInfo: async (): Promise<LanAccessInfo> => {
    // 后端返回实时网卡和服务状态，前端不缓存旧地址。
    return call<LanAccessInfo>('lanAccess.getInfo')
  },

  resetLanAccessToken: async (): Promise<LanAccessInfo> => {
    // 令牌轮换由后端完成，并返回新的完整链接状态。
    return call<LanAccessInfo>('lanAccess.resetToken')
  },
}

export interface LanNetworkAddress {
  adapterId: string // 稳定适配器标识，用于保存首选项
  adapterName: string // Windows 友好名称
  ip: string // 可供其他设备访问的 IPv4 地址
  metric: number // Windows IPv4 接口 metric
  hasDefaultGateway: boolean // 是否存在 IPv4 默认网关
  isVirtual: boolean // 是否由 Windows 接口类型明确标记为虚拟接口
  isPrivate: boolean // 是否属于 RFC1918 局域网私有地址段
  isPreferred: boolean // 是否命中用户首选适配器
  url: string // 当前令牌有效时生成的访问链接
}

// LAN 设置页展示的服务状态、令牌链接和网卡排序信息。
export interface LanAccessInfo {
  configuredEnabled: boolean // 设置中是否启用 LAN
  runtimeEnabled: boolean // 当前进程是否已按 LAN 范围监听
  restartRequired: boolean // 配置与运行状态是否不一致
  port: number // 当前实际监听端口
  preferredUrl: string // 排序后的首选访问链接
  addresses: LanNetworkAddress[] // 所有可用地址及其链接
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

// 仅在本机页面打开原生目录选择框，远端调用直接拒绝。
export async function selectDirectory(title: string): Promise<string | null> {
  if (!isLocalAccess()) {
    throw new Error('Directory selection is only available in the local application window.')
  }

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
    // 背景图片选择同样需要访问宿主机文件系统。
    if (!isLocalAccess()) {
      throw new Error('File selection is only available in the local application window.')
    }

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
    console.error('Failed to select background image:', error)
    throw error
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
    console.error('Failed to import background image:', error)
    throw error
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
    console.warn('Failed to clean up old background image:', error)
  }
}
