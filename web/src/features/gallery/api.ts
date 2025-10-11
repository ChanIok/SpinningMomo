import { call } from '@/core/rpc'
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
  FolderTreeNode,
  GetTimelineBucketsParams,
  TimelineBucketsResponse,
  GetAssetsByMonthParams,
  GetAssetsByMonthResponse,
} from './types'
import { getStaticUrl } from '@/core/env'

/**
 * Gallery API 层
 * 基于 core/rpc 封装业务语义化的接口
 */

/**
 * 获取单个资产详情
 */
export async function getAsset(params: GetAssetParams): Promise<Asset> {
  try {
    const result = await call<Asset>('gallery.get', params)

    console.log('📸 获取资产详情成功:', result.name)

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

    const result = await call<OperationResult>('gallery.delete', params)

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
    const result = await call<AssetStats>('gallery.stats', {})

    console.log('📋 获取资产统计成功:', result)

    return result
  } catch (error) {
    console.error('Failed to get asset stats:', error)
    throw new Error('获取资产统计失败')
  }
}

/**
 * 获取文件夹树结构
 */
export async function getFolderTree(): Promise<FolderTreeNode[]> {
  try {
    const result = await call<FolderTreeNode[]>('gallery.getFolderTree', {})

    console.log('📁 获取文件夹树成功:', result.length, '个根文件夹')

    return result
  } catch (error) {
    console.error('Failed to get folder tree:', error)
    throw new Error('获取文件夹树失败')
  }
}

/**
 * 获取资产列表（可按文件夹筛选，可选包含子文件夹）
 */
export async function listAssets(params: ListAssetsParams = {}): Promise<ListAssetsResponse> {
  try {
    const result = await call<ListAssetsResponse>('gallery.listAssets', params)

    console.log('📸 获取资产列表成功:', {
      count: result.items.length,
      total: result.totalCount,
      page: result.currentPage,
      folderId: params.folderId,
      includeSubfolders: params.includeSubfolders,
    })

    return result
  } catch (error) {
    console.error('Failed to list assets:', error)
    throw new Error('获取资产列表失败')
  }
}

/**
 * 扫描资产目录
 */
export async function scanAssets(params: ScanAssetsParams): Promise<ScanAssetsResult> {
  try {
    console.log('🔍 开始扫描资产目录:', params.directories)

    const result = await call<ScanAssetsResult>('gallery.scanDirectory', params)

    console.log('✅ 资产扫描完成:', {
      total: result.totalFiles,
      new: result.newItems,
      updated: result.updatedItems,
      duration: result.scanDuration,
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

    const result = await call<OperationResult>('gallery.cleanupThumbnails', {})

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
    const result = await call<string>('gallery.thumbnailStats', {})

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

    const result = await call<OperationResult>('gallery.cleanupDeleted', { daysOld: daysOld })

    console.log('✅ 已删除资产清理完成:', result.message)

    return result
  } catch (error) {
    console.error('Failed to cleanup deleted assets:', error)
    throw new Error('清理已删除资产失败')
  }
}

/**
 * 获取资产缩略图URL - 从 asset对象直接构建
 * 路径格式: thumbnails/[hash前2位]/[hash第3-4位]/{hash}.webp
 */
export function getAssetThumbnailUrl(asset: Asset): string {
  const hash = asset.hash
  if (!hash) {
    return ''
  }

  const prefix1 = hash.slice(0, 2)
  const prefix2 = hash.slice(2, 4)

  return getStaticUrl(`/static/thumbnails/${prefix1}/${prefix2}/${hash}.webp`)
}

/**
 * 获取资产原图URL - 使用HTTP接口
 */
export function getAssetUrl(assetId: number): string {
  // TODO: 后续添加HTTP接口后更换为真实URL
  // 目前使用mock数据
  const seed = assetId % 1000
  return `https://picsum.photos/seed/${seed}/1920/1080`
}

/**
 * 获取时间线桶（月份元数据）
 */
export async function getTimelineBuckets(
  params: GetTimelineBucketsParams = {}
): Promise<TimelineBucketsResponse> {
  try {
    const result = await call<TimelineBucketsResponse>('gallery.getTimelineBuckets', params)

    console.log('📅 获取时间线桶成功:', {
      buckets: result.buckets.length,
      totalCount: result.totalCount,
      folderId: params.folderId,
    })

    return result
  } catch (error) {
    console.error('Failed to get timeline buckets:', error)
    throw new Error('获取时间线桶失败')
  }
}

/**
 * 获取指定月份的资产
 */
export async function getAssetsByMonth(
  params: GetAssetsByMonthParams
): Promise<GetAssetsByMonthResponse> {
  try {
    const result = await call<GetAssetsByMonthResponse>('gallery.getAssetsByMonth', params)

    console.log('📸 获取月份资产成功:', {
      month: result.month,
      count: result.count,
      folderId: params.folderId,
    })

    return result
  } catch (error) {
    console.error('Failed to get assets by month:', error)
    throw new Error('获取月份资产失败')
  }
}

/**
 * Gallery API 统一导出
 */
export const galleryApi = {
  // 数据查询
  listAssets,
  getAsset,
  getAssetStats,
  getFolderTree,

  // 时间线查询
  getTimelineBuckets,
  getAssetsByMonth,

  // 数据操作
  deleteAsset,
  scanAssets,

  // 维护操作
  cleanupThumbnails,
  getThumbnailStats,
  cleanupDeletedAssets,

  // URL 工具
  getAssetThumbnailUrl,
  getAssetUrl,
}
