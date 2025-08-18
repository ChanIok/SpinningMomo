import { call, JsonRpcError } from '@/lib/rpc'

/**
 * 检查更新
 */
export async function checkForUpdates() {
  try {
    const result = await call<{
      has_update: boolean
      latest_version: string
      download_url: string
      changelog?: string
    }>('updater.check_for_update')

    return {
      available: result.has_update,
      version: result.latest_version,
      downloadUrl: result.download_url,
      releaseNotes: result.changelog,
    }
  } catch (error) {
    console.error('Failed to check for updates:', error)
    if (error instanceof JsonRpcError) {
      throw new Error(`检查更新失败: ${error.message}`)
    }
    throw new Error('检查更新失败')
  }
}

/**
 * 下载更新
 */
export async function downloadUpdate(downloadUrl: string) {
  try {
    const result = await call<{
      file_path: string
      message: string
    }>('updater.download_update', {
      download_url: downloadUrl,
    })

    return {
      success: true,
      message: result.message,
      filePath: result.file_path,
    }
  } catch (error) {
    console.error('Failed to download update:', error)
    if (error instanceof JsonRpcError) {
      throw new Error(`下载更新失败: ${error.message}`)
    }
    throw new Error('下载更新失败')
  }
}

/**
 * 安装更新
 */
export async function installUpdate(restart: boolean = true) {
  try {
    const result = await call<{
      message: string
    }>('updater.install_update', {
      restart,
    })

    return {
      success: true,
      message: result.message,
    }
  } catch (error) {
    console.error('Failed to install update:', error)
    if (error instanceof JsonRpcError) {
      throw new Error(`安装更新失败: ${error.message}`)
    }
    throw new Error('安装更新失败')
  }
}
