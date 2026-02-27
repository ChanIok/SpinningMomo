import { call } from '@/core/rpc'
import type {
  Asset,
  ListAssetsParams,
  ListAssetsResponse,
  OperationResult,
  ScanAssetsParams,
  ScanAssetsResult,
  FolderTreeNode,
  GetTimelineBucketsParams,
  TimelineBucketsResponse,
  GetAssetsByMonthParams,
  GetAssetsByMonthResponse,
  QueryAssetsParams,
  QueryAssetsResponse,
  Tag,
  TagTreeNode,
  TagStats,
  CreateTagParams,
  UpdateTagParams,
  AddTagsToAssetParams,
  RemoveTagsFromAssetParams,
} from './types'
import { getStaticUrl } from '@/core/env'
import { transformInfinityNikkiTree } from '@/plugins/infinity_nikki'

/**
 * 获取文件夹树结构
 */
export async function getFolderTree(): Promise<FolderTreeNode[]> {
  try {
    const result = await call<FolderTreeNode[]>('gallery.getFolderTree', {})

    console.log('📁 获取文件夹树成功:', result.length, '个根文件夹')

    // 应用 InfinityNikki 插件转换
    const transformedResult = transformInfinityNikkiTree(result)

    return transformedResult
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
    console.log('🔍 开始扫描资产目录:', params.directory)

    const result = await call<ScanAssetsResult>('gallery.scanDirectory', params, 0)

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

  return getStaticUrl(`/static/assets/thumbnails/${prefix1}/${prefix2}/${hash}.webp`)
}

/**
 * 获取资产原图URL
 */
export function getAssetUrl(assetId: number): string {
  return getStaticUrl(`/static/assets/originals/${assetId}`)
}

/**
 * 获取时间线桶（月份元数据）
 */
export async function getTimelineBuckets(
  params: GetTimelineBucketsParams = {}
): Promise<TimelineBucketsResponse> {
  try {
    const result = await call<TimelineBucketsResponse>('gallery.getTimelineBuckets', params)

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

    return result
  } catch (error) {
    console.error('Failed to get assets by month:', error)
    throw new Error('获取月份资产失败')
  }
}

/**
 * 统一资产查询接口（支持灵活过滤器和可选分页）
 */
export async function queryAssets(params: QueryAssetsParams): Promise<QueryAssetsResponse> {
  try {
    const result = await call<QueryAssetsResponse>('gallery.queryAssets', params)

    console.log('🔍 查询资产成功:', {
      count: result.items.length,
      total: result.totalCount,
      page: result.currentPage,
      filters: params.filters,
    })

    return result
  } catch (error) {
    console.error('Failed to query assets:', error)
    throw new Error('查询资产失败')
  }
}

/**
 * 获取标签树结构
 */
export async function getTagTree(): Promise<TagTreeNode[]> {
  try {
    const result = await call<TagTreeNode[]>('gallery.getTagTree', {})

    console.log('🏷️ 获取标签树成功:', result.length, '个根标签')

    return result
  } catch (error) {
    console.error('Failed to get tag tree:', error)
    throw new Error('获取标签树失败')
  }
}

/**
 * 获取所有标签（扫平列表）
 */
export async function listTags(): Promise<Tag[]> {
  try {
    const result = await call<Tag[]>('gallery.listTags', {})

    console.log('🏷️ 获取标签列表成功:', result.length, '个标签')

    return result
  } catch (error) {
    console.error('Failed to list tags:', error)
    throw new Error('获取标签列表失败')
  }
}

/**
 * 创建标签
 */
export async function createTag(params: CreateTagParams): Promise<{ id: number }> {
  try {
    console.log('➕ 创建标签:', params.name)

    const result = await call<number>('gallery.createTag', params)

    console.log('✅ 标签创建成功:', result)

    return { id: result }
  } catch (error) {
    console.error('Failed to create tag:', error)
    throw new Error('创建标签失败')
  }
}

/**
 * 更新标签
 */
export async function updateTag(params: UpdateTagParams): Promise<OperationResult> {
  try {
    console.log('✏️ 更新标签:', params.id)

    const result = await call<OperationResult>('gallery.updateTag', params)

    console.log('✅ 标签更新成功:', result.message)

    return result
  } catch (error) {
    console.error('Failed to update tag:', error)
    throw new Error('更新标签失败')
  }
}

/**
 * 删除标签
 */
export async function deleteTag(tagId: number): Promise<OperationResult> {
  try {
    console.log('🗑️ 删除标签:', tagId)

    const result = await call<OperationResult>('gallery.deleteTag', { id: tagId })

    console.log('✅ 标签删除成功:', result.message)

    return result
  } catch (error) {
    console.error('Failed to delete tag:', error)
    throw new Error('删除标签失败')
  }
}

/**
 * 获取标签统计
 */
export async function getTagStats(): Promise<TagStats[]> {
  try {
    const result = await call<TagStats[]>('gallery.getTagStats', {})

    console.log('📊 获取标签统计成功')

    return result
  } catch (error) {
    console.error('Failed to get tag stats:', error)
    throw new Error('获取标签统计失败')
  }
}

/**
 * 为资产添加标签
 */
export async function addTagsToAsset(params: AddTagsToAssetParams): Promise<OperationResult> {
  try {
    console.log('🏷️ 为资产添加标签:', params.assetId, params.tagIds)

    const result = await call<OperationResult>('gallery.addTagsToAsset', params)

    console.log('✅ 标签添加成功:', result.message)

    return result
  } catch (error) {
    console.error('Failed to add tags to asset:', error)
    throw new Error('添加标签失败')
  }
}

/**
 * 从资产移除标签
 */
export async function removeTagsFromAsset(
  params: RemoveTagsFromAssetParams
): Promise<OperationResult> {
  try {
    console.log('🗑️ 从资产移除标签:', params.assetId, params.tagIds)

    const result = await call<OperationResult>('gallery.removeTagsFromAsset', params)

    console.log('✅ 标签移除成功:', result.message)

    return result
  } catch (error) {
    console.error('Failed to remove tags from asset:', error)
    throw new Error('移除标签失败')
  }
}

/**
 * 获取资产的所有标签
 */
export async function getAssetTags(assetId: number): Promise<Tag[]> {
  try {
    const result = await call<Tag[]>('gallery.getAssetTags', { assetId })

    return result
  } catch (error) {
    console.error('Failed to get asset tags:', error)
    throw new Error('获取资产标签失败')
  }
}

/**
 * 批量获取多个资产的标签
 */
export async function getTagsByAssetIds(assetIds: number[]): Promise<Record<number, Tag[]>> {
  try {
    const result = await call<Record<number, Tag[]>>('gallery.getTagsByAssetIds', { assetIds })

    return result
  } catch (error) {
    console.error('Failed to get tags by asset ids:', error)
    throw new Error('批量获取资产标签失败')
  }
}

/**
 * Gallery API 统一导出
 */
export const galleryApi = {
  // 数据查询
  listAssets,
  getFolderTree,
  queryAssets, // 统一查询接口

  // 时间线查询
  getTimelineBuckets,
  getAssetsByMonth,

  // 数据操作
  scanAssets,

  // 维护操作
  cleanupThumbnails,
  getThumbnailStats,
  cleanupDeletedAssets,

  // 标签管理
  getTagTree,
  listTags,
  createTag,
  updateTag,
  deleteTag,
  getTagStats,

  // 资产-标签关联
  addTagsToAsset,
  removeTagsFromAsset,
  getAssetTags,
  getTagsByAssetIds,

  // URL 工具
  getAssetThumbnailUrl,
  getAssetUrl,
}
