import { call } from '@/lib/rpc'
import type {
  Asset,
  ListAssetsParams,
  ListAssetsResponse,
  GetAssetParams,
  DeleteAssetParams,
  OperationResult,
  AssetStats,
  ScanAssetsParams,
  ScanAssetsResult,
} from './types'

/**
 * 获取资产列表
 */
export async function listAssets(params: ListAssetsParams = {}): Promise<ListAssetsResponse> {
  try {
    const result = await call<ListAssetsResponse>('asset.list', params)

    console.log('📸 获取资产列表成功:', {
      count: result.items.length,
      total: result.total_count,
      page: result.current_page,
    })

    return result
  } catch (error) {
    console.error('Failed to list assets:', error)
    throw new Error('获取资产列表失败')
  }
}

/**
 * 获取单个资产详情
 */
export async function getAsset(params: GetAssetParams): Promise<Asset> {
  try {
    const result = await call<Asset>('asset.get', params)

    console.log('📸 获取资产详情成功:', result.filename)

    return result
  } catch (error) {
    console.error('Failed to get asset:', error)
    throw new Error('获取资产详情失败')
  }
}

/**
 * 删除资产
 */
export async function deleteAsset(params: DeleteAssetParams): Promise<OperationResult> {
  try {
    console.log('📸 删除资产:', params)

    const result = await call<OperationResult>('asset.delete', params)

    console.log('✅ 资产删除成功:', result.message)

    return result
  } catch (error) {
    console.error('Failed to delete asset:', error)
    throw new Error('删除资产失败')
  }
}

/**
 * 获取资产统计信息
 */
export async function getAssetStats(): Promise<AssetStats> {
  try {
    const result = await call<AssetStats>('asset.stats', {})

    console.log('📊 获取资产统计成功:', result)

    return result
  } catch (error) {
    console.error('Failed to get asset stats:', error)
    throw new Error('获取资产统计失败')
  }
}

/**
 * 扫描资产目录
 */
export async function scanAssets(params: ScanAssetsParams): Promise<ScanAssetsResult> {
  try {
    console.log('🔍 开始扫描资产目录:', params.directories)

    const result = await call<ScanAssetsResult>('asset.scan', params)

    console.log('✅ 资产扫描完成:', {
      total: result.total_files,
      new: result.new_items,
      updated: result.updated_items,
      duration: result.scan_duration,
    })

    return result
  } catch (error) {
    console.error('Failed to scan assets:', error)
    throw new Error('扫描资产目录失败')
  }
}

/**
 * 清理缩略图
 */
export async function cleanupThumbnails(): Promise<OperationResult> {
  try {
    console.log('🧹 开始清理缩略图')

    const result = await call<OperationResult>('asset.cleanupThumbnails', {})

    console.log('✅ 缩略图清理完成:', result.message)

    return result
  } catch (error) {
    console.error('Failed to cleanup thumbnails:', error)
    throw new Error('清理缩略图失败')
  }
}

/**
 * 获取缩略图统计
 */
export async function getThumbnailStats(): Promise<string> {
  try {
    const result = await call<string>('asset.thumbnailStats', {})

    console.log('📊 获取缩略图统计成功')

    return result
  } catch (error) {
    console.error('Failed to get thumbnail stats:', error)
    throw new Error('获取缩略图统计失败')
  }
}

/**
 * 清理已删除资产
 */
export async function cleanupDeletedAssets(daysOld = 30): Promise<OperationResult> {
  try {
    console.log('🧹 清理已删除资产:', { daysOld })

    const result = await call<OperationResult>('asset.cleanupDeleted', { days_old: daysOld })

    console.log('✅ 已删除资产清理完成:', result.message)

    return result
  } catch (error) {
    console.error('Failed to cleanup deleted assets:', error)
    throw new Error('清理已删除资产失败')
  }
}

/**
 * 获取资产缩略图URL
 */
export function getAssetThumbnailUrl(assetId: number, width = 400, height = 400): string {
  // 假设后端提供了缩略图URL的端点
  return `/api/thumbnails/${assetId}?w=${width}&h=${height}`
}

/**
 * 获取资产原图URL
 */
export function getAssetUrl(assetId: number): string {
  return `/api/assets/${assetId}/file`
}
