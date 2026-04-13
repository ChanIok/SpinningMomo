import { call } from '@/core/rpc'
import type {
  Asset,
  ListAssetsParams,
  ListAssetsResponse,
  OperationResult,
  ScanAssetsParams,
  ScanAssetsResult,
  StartScanAssetsResult,
  FolderTreeNode,
  UpdateFolderDisplayNameParams,
  GetTimelineBucketsParams,
  TimelineBucketsResponse,
  GetAssetsByMonthParams,
  GetAssetsByMonthResponse,
  QueryAssetsParams,
  QueryAssetsResponse,
  QueryAssetLayoutMetaParams,
  QueryAssetLayoutMetaResponse,
  QueryPhotoMapPointsParams,
  PhotoMapPoint,
  InfinityNikkiDetails,
  AssetMainColor,
  Tag,
  TagTreeNode,
  TagStats,
  HomeStats,
  CreateTagParams,
  UpdateTagParams,
  AddTagsToAssetParams,
  RemoveTagsFromAssetParams,
  UpdateAssetsReviewStateParams,
  UpdateAssetDescriptionParams,
  SetInfinityNikkiUserRecordParams,
  AssetReachability,
} from './types'
import { getStaticUrl, isWebView } from '@/core/env'
import { transformInfinityNikkiTree } from '@/extensions/infinity_nikki'
import { useI18n } from '@/core/i18n'

/**
 * 转换默认输出文件夹树结构
 * 将 SpinningMomo 根文件夹的显示名称设置为应用名称
 * @param tree 原始文件夹树
 * @returns 转换后的文件夹树
 */
export function transformDefaultOutputFolderTree(tree: FolderTreeNode[]): FolderTreeNode[] {
  const { t } = useI18n()

  // 遍历并转换所有根文件夹
  return tree.map((node) => {
    // 检查是否是 SpinningMomo 根文件夹
    if (node.name === 'SpinningMomo' && !node.displayName) {
      return {
        ...node,
        displayName: t('app.name'),
      }
    }
    return node
  })
}

/**
 * 获取文件夹树结构
 */
export async function getFolderTree(): Promise<FolderTreeNode[]> {
  try {
    const result = await call<FolderTreeNode[]>('gallery.getFolderTree', {})

    console.log('📁 获取文件夹树成功:', result.length, '个根文件夹')

    // 应用默认输出文件夹转换
    let transformedResult = transformDefaultOutputFolderTree(result)

    // 应用 Infinity Nikki 拓展转换
    transformedResult = transformInfinityNikkiTree(transformedResult)

    return transformedResult
  } catch (error) {
    console.error('Failed to get folder tree:', error)
    throw new Error('获取文件夹树失败')
  }
}

/**
 * 更新文件夹显示名称（仅应用内）
 */
export async function updateFolderDisplayName(
  params: UpdateFolderDisplayNameParams
): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.updateFolderDisplayName', params)
    return result
  } catch (error) {
    console.error('Failed to update folder display name:', error)
    throw new Error('更新文件夹显示名称失败')
  }
}

/**
 * 在资源管理器中打开文件夹
 */
export async function openFolderInExplorer(folderId: number): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.openFolderInExplorer', { id: folderId })
    return result
  } catch (error) {
    console.error('Failed to open folder in explorer:', error)
    throw new Error('打开文件夹失败')
  }
}

/**
 * 移出根文件夹监听并清理索引
 */
export async function removeFolderWatch(folderId: number): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.removeFolderWatch', { id: folderId })
    return result
  } catch (error) {
    console.error('Failed to remove folder watch:', error)
    throw new Error('移出监听失败')
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
 * 在后台启动资产扫描任务
 */
export async function startScanAssets(params: ScanAssetsParams): Promise<StartScanAssetsResult> {
  try {
    console.log('🧵 提交后台扫描任务:', params.directory)

    const result = await call<StartScanAssetsResult>('gallery.startScanDirectory', params)

    console.log('✅ 后台扫描任务已创建:', result.taskId)

    return result
  } catch (error) {
    console.error('Failed to start scan task:', error)
    throw new Error('提交扫描任务失败')
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
  const relativePath = `${prefix1}/${prefix2}/${hash}.webp`

  // WebView release 直接走缩略图虚拟主机映射，少一层动态解析。
  if (isWebView() && !import.meta.env.DEV) {
    return `https://thumbs.test/${relativePath}`
  }

  return getStaticUrl(`/static/assets/thumbnails/${relativePath}`)
}
function encodeRelativePathForUrl(relativePath: string): string {
  // 逐段编码，而不是把整个路径一起编码。
  // 这样可以保留目录层级里的 '/'，同时正确处理中文、空格、#、% 等特殊字符。
  return relativePath
    .split('/')
    .filter((segment) => segment.length > 0)
    .map((segment) => encodeURIComponent(segment))
    .join('/')
}

/**
 * 获取资产原图 URL。
 *
 * 新模型下，原图不再通过 assetId 间接解析，而是直接由：
 * - `rootId`：资源属于哪个 watch root
 * - `relativePath`：文件在该 root 下的相对路径
 * - `hash`：作为版本参数，避免内容更新后 URL 不变
 *
 * 环境差异：
 * - WebView：`https://r-<rootId>.test/<relativePath>?v=<hash>`
 * - 浏览器 dev：`/static/assets/originals/by-root/<rootId>/<relativePath>?v=<hash>`
 */
export function getAssetUrl(asset: Asset): string {
  if (!asset.rootId || !asset.relativePath) {
    return ''
  }

  const encodedRelativePath = encodeRelativePathForUrl(asset.relativePath)
  const versionQuery = asset.hash ? `?v=${encodeURIComponent(asset.hash)}` : ''

  if (isWebView()) {
    return `https://r-${asset.rootId}.test/${encodedRelativePath}${versionQuery}`
  }

  return `/static/assets/originals/by-root/${asset.rootId}/${encodedRelativePath}${versionQuery}`
}

/**
 * 使用系统默认应用打开资产文件
 */
export async function openAssetDefault(assetId: number): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.openAssetDefault', { id: assetId })
    return result
  } catch (error) {
    console.error('Failed to open asset with default app:', error)
    throw new Error('打开文件失败')
  }
}

/**
 * 在资源管理器中显示并选中资产文件
 */
export async function revealAssetInExplorer(assetId: number): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.revealAssetInExplorer', { id: assetId })
    return result
  } catch (error) {
    console.error('Failed to reveal asset in explorer:', error)
    throw new Error('在资源管理器中定位文件失败')
  }
}

/**
 * 将资产移动到系统回收站
 */
export async function moveAssetsToTrash(assetIds: number[]): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.moveAssetsToTrash', { ids: assetIds })
    return result
  } catch (error) {
    console.error('Failed to move assets to trash:', error)
    throw new Error('移到回收站失败')
  }
}

export async function checkAssetReachable(assetId: number): Promise<AssetReachability> {
  try {
    const result = await call<AssetReachability>('gallery.checkAssetReachable', { assetId })
    return result
  } catch (error) {
    console.error('Failed to check asset reachability:', error)
    throw new Error('检查资产可达性失败')
  }
}

/**
 * 批量更新资产的审片状态（评分 / 留用 / 弃置）
 */
export async function updateAssetsReviewState(
  params: UpdateAssetsReviewStateParams
): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.updateAssetsReviewState', params)
    return result
  } catch (error) {
    console.error('Failed to update assets review state:', error)
    throw new Error('更新审片状态失败')
  }
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
      activeAssetIndex: result.activeAssetIndex,
      filters: params.filters,
    })

    return result
  } catch (error) {
    console.error('Failed to query assets:', error)
    throw new Error('查询资产失败')
  }
}

/**
 * 查询自适应视图需要的轻量布局元数据
 */
export async function queryAssetLayoutMeta(
  params: QueryAssetLayoutMetaParams
): Promise<QueryAssetLayoutMetaResponse> {
  try {
    const result = await call<QueryAssetLayoutMetaResponse>('gallery.queryAssetLayoutMeta', params)

    console.log('🧩 查询布局元数据成功:', {
      count: result.items.length,
      total: result.totalCount,
      filters: params.filters,
    })

    return result
  } catch (error) {
    console.error('Failed to query asset layout meta:', error)
    throw new Error('查询布局元数据失败')
  }
}

/**
 * 查询当前筛选下的地图点位
 */
export async function queryPhotoMapPoints(
  params: QueryPhotoMapPointsParams
): Promise<PhotoMapPoint[]> {
  try {
    const result = await call<PhotoMapPoint[]>('gallery.queryPhotoMapPoints', params)

    console.log('🗺️ 查询地图点位成功:', {
      count: result.length,
      filters: params.filters,
    })

    return result
  } catch (error) {
    console.error('Failed to query photo map points:', error)
    throw new Error('查询地图点位失败')
  }
}

/**
 * 获取 Infinity Nikki 详情
 */
export async function getInfinityNikkiDetails(assetId: number): Promise<InfinityNikkiDetails> {
  try {
    const result = await call<InfinityNikkiDetails>('gallery.getInfinityNikkiDetails', {
      assetId,
    })

    return result
  } catch (error) {
    console.error('Failed to get Infinity Nikki details:', error)
    throw new Error('获取无限暖暖详情失败')
  }
}

/**
 * 获取资产主色调板
 */
export async function getAssetMainColors(assetId: number): Promise<AssetMainColor[]> {
  try {
    const result = await call<AssetMainColor[]>('gallery.getAssetMainColors', {
      assetId,
    })

    return result
  } catch (error) {
    console.error('Failed to get asset main colors:', error)
    throw new Error('获取主色失败')
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
 * 获取首页统计摘要
 */
export async function getHomeStats(): Promise<HomeStats> {
  try {
    const result = await call<HomeStats>('gallery.getHomeStats', {})

    return result
  } catch (error) {
    console.error('Failed to get home stats:', error)
    throw new Error('获取首页统计失败')
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
 * 更新资产描述
 */
export async function updateAssetDescription(
  params: UpdateAssetDescriptionParams
): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.updateAssetDescription', params)

    return result
  } catch (error) {
    console.error('Failed to update asset description:', error)
    throw new Error('更新资产描述失败')
  }
}

/**
 * 设置 Infinity Nikki 玩家记录
 */
export async function setInfinityNikkiUserRecord(
  params: SetInfinityNikkiUserRecordParams
): Promise<OperationResult> {
  try {
    const result = await call<OperationResult>('gallery.setInfinityNikkiUserRecord', params)

    return result
  } catch (error) {
    console.error('Failed to set Infinity Nikki user record:', error)
    throw new Error('更新无限暖暖玩家记录失败')
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
  updateFolderDisplayName,
  openFolderInExplorer,
  removeFolderWatch,
  queryAssets, // 统一查询接口
  queryAssetLayoutMeta,
  queryPhotoMapPoints,
  getInfinityNikkiDetails,
  getAssetMainColors,

  // 时间线查询
  getTimelineBuckets,
  getAssetsByMonth,

  // 数据操作
  scanAssets,
  startScanAssets,

  // 维护操作
  cleanupThumbnails,
  getThumbnailStats,

  // 标签管理
  getTagTree,
  listTags,
  createTag,
  updateTag,
  deleteTag,
  getTagStats,
  getHomeStats,

  // 资产-标签关联
  addTagsToAsset,
  removeTagsFromAsset,
  getAssetTags,
  getTagsByAssetIds,

  // URL 工具
  getAssetThumbnailUrl,
  getAssetUrl,

  // 资产动作
  openAssetDefault,
  revealAssetInExplorer,
  moveAssetsToTrash,
  checkAssetReachable,
  updateAssetsReviewState,
  updateAssetDescription,
  setInfinityNikkiUserRecord,
}
