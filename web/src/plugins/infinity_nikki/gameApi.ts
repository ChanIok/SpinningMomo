import { call } from '@/lib/rpc'
import type { InfinityNikkiGameDirResult, InfinityNikkiScanResult } from './types'
import { createScanConfig } from './scanConfig'

/**
 * 获取Infinity Nikki游戏安装目录
 *
 * @returns 游戏目录获取结果
 */
export async function getInfinityNikkiGameDirectory(): Promise<InfinityNikkiGameDirResult> {
  try {
    console.log('🎮 正在获取Infinity Nikki游戏目录...')

    const result = await call<InfinityNikkiGameDirResult>(
      'plugins.infinityNikki.getGameDirectory',
      {}
    )

    console.log('🎮 游戏目录获取结果:', {
      found: result.gameDirFound,
      directory: result.gameDir,
      message: result.message,
    })

    return result
  } catch (error) {
    console.error('❌ 获取游戏目录失败:', error)
    throw new Error('无法获取Infinity Nikki游戏目录')
  }
}

/**
 * 扫描Infinity Nikki照片
 *
 * 完整的扫描流程：
 * 1. 获取游戏安装目录
 * 2. 验证目录有效性
 * 3. 生成扫描配置（包含ignore规则）
 * 4. 执行扫描
 *
 * @returns 扫描结果
 */
export async function scanInfinityNikkiPhotos(): Promise<InfinityNikkiScanResult> {
  try {
    console.log('📸 开始扫描Infinity Nikki照片...')

    // 1. 获取游戏目录
    const gameDirResult = await getInfinityNikkiGameDirectory()

    if (!gameDirResult.gameDirFound || !gameDirResult.gameDir) {
      return {
        success: false,
        error: `无法找到游戏目录: ${gameDirResult.message}`,
      }
    }

    const gameDirectory = gameDirResult.gameDir
    console.log('📁 游戏目录:', gameDirectory)

    // 2. 生成扫描配置
    const scanConfig = createScanConfig(gameDirectory)
    console.log('⚙️ 扫描配置:', {
      directory: scanConfig.directory,
      ignore_rules_count: scanConfig.ignore_rules.length,
    })

    // 3. 执行扫描
    const scanResult = await call<{
      totalFiles: number
      newItems: number
      updatedItems: number
      deletedItems: number
      errors: string[]
      scanDuration: string
    }>('gallery.scanDirectory', scanConfig, 0)

    return {
      success: true,
      gameDirectory: gameDirectory,
      scanResult: scanResult,
    }
  } catch (error) {
    console.error('❌ 扫描失败:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : '未知错误',
    }
  }
}
