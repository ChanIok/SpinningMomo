import { call, callWithAdditionalObjects } from '@/core/rpc'
import type { FolderTreeNode, Tag, TagTreeNode } from './types'
import { getAssetThumbnailUrl, getAssetUrl } from './api/urls'
import type {
  OperationResult,
  ScanAssetsParams,
  ScanAssetsResult,
  StartScanAssetsResult,
  CreateFolderParams,
  UpdateFolderDisplayNameParams,
  GetTimelineBucketsParams,
  TimelineBucketsResponse,
  GetAssetsByMonthParams,
  GetAssetsByMonthResponse,
  QueryAssetsParams,
  QueryAssetsResponse,
  QueryAssetLayoutMetaParams,
  QueryAssetLayoutMetaResponse,
  AssetMainColor,
  TagStats,
  HomeStats,
  CreateTagParams,
  UpdateTagParams,
  AddTagsToAssetParams,
  AddTagToAssetsParams,
  RemoveTagFromAssetsParams,
  RemoveTagsFromAssetParams,
  UpdateAssetsReviewStateParams,
  MoveAssetsToFolderParams,
  UpdateAssetDescriptionParams,
  UpdateAssetsDescriptionParams,
  AssetReachability,
  BatchSelectionSummary,
  MissingAssetsResponse,
  PurgeMissingAssetsParams,
  PurgeMissingAssetsResult,
  DeleteAssetsParams,
  DeleteAssetsResult,
} from './api/dto'
import { transformInfinityNikkiTree } from '@/extensions/infinity_nikki'
import { useI18n } from '@/core/i18n'
import { isLocalAccess } from '@/core/access'

// 在发起可能触及宿主机的图库操作前，先阻止 LAN 页面进入本机 API。
function requireLocalAccess(action: string): void {
  if (!isLocalAccess()) {
    throw new Error(`${action} is only available in the local application window.`)
  }
}

// 以下接口在后端也受 local 权限保护；前端检查用于避免误触发和显示明确错误。

// 为内置输出目录补充本地化显示名称，同时保留用户设置的名称。
export function transformDefaultOutputFolderTree(tree: FolderTreeNode[]): FolderTreeNode[] {
  const { t } = useI18n()

  return tree.map((node) => {
    if (node.name === 'SpinningMomo' && !node.displayName) {
      return {
        ...node,
        displayName: t('app.name'),
      }
    }
    return node
  })
}

export async function getFolderTree(): Promise<FolderTreeNode[]> {
  const tree = await call<FolderTreeNode[]>('gallery.getFolderTree', {})
  return transformInfinityNikkiTree(transformDefaultOutputFolderTree(tree))
}

export function createFolder(params: CreateFolderParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.createFolder', params)
}

export function updateFolderDisplayName(
  params: UpdateFolderDisplayNameParams
): Promise<OperationResult> {
  return call<OperationResult>('gallery.updateFolderDisplayName', params)
}

// 在 Windows 资源管理器中打开图库文件夹。
export function openFolderInExplorer(folderId: number): Promise<OperationResult> {
  requireLocalAccess('Opening a folder in Explorer')
  return call<OperationResult>('gallery.openFolderInExplorer', { id: folderId })
}

// 移除根目录监听并让后端清理对应索引。
export function removeFolderWatch(folderId: number): Promise<OperationResult> {
  requireLocalAccess('Removing a folder watch')
  return call<OperationResult>('gallery.removeFolderWatch', { id: folderId })
}

// 同步扫描指定目录，调用者通常只在本机设置流程中使用。
export function scanAssets(params: ScanAssetsParams): Promise<ScanAssetsResult> {
  requireLocalAccess('Scanning a directory')
  return call<ScanAssetsResult>('gallery.scanDirectory', params, 0)
}

// 创建后台扫描任务并返回任务标识。
export function startScanAssets(params: ScanAssetsParams): Promise<StartScanAssetsResult> {
  requireLocalAccess('Scanning a directory')
  return call<StartScanAssetsResult>('gallery.startScanDirectory', params)
}

export function cleanupThumbnails(): Promise<OperationResult> {
  return call<OperationResult>('gallery.cleanupThumbnails', {})
}

export function getThumbnailStats(): Promise<string> {
  return call<string>('gallery.thumbnailStats', {})
}

export function getMissingAssets(): Promise<MissingAssetsResponse> {
  return call<MissingAssetsResponse>('gallery.getMissingAssets', {})
}

export function purgeMissingAssets(
  params: PurgeMissingAssetsParams = {}
): Promise<PurgeMissingAssetsResult> {
  return call<PurgeMissingAssetsResult>('gallery.purgeMissingAssets', params)
}

// 使用系统默认程序打开指定资产。
export function openAssetDefault(assetId: number): Promise<OperationResult> {
  requireLocalAccess('Opening an asset')
  return call<OperationResult>('gallery.openAssetDefault', { id: assetId })
}

// 在资源管理器中定位指定资产文件。
export function revealAssetInExplorer(assetId: number): Promise<OperationResult> {
  requireLocalAccess('Revealing an asset in Explorer')
  return call<OperationResult>('gallery.revealAssetInExplorer', { id: assetId })
}

// 将选中的资产文件复制到宿主机剪贴板。
export function copyAssetsToClipboard(assetIds: number[]): Promise<OperationResult> {
  requireLocalAccess('Copying assets to the clipboard')
  return call<OperationResult>('gallery.copyAssetsToClipboard', { ids: assetIds })
}

// 读取宿主机剪贴板文件并导入指定图库文件夹。
export function pasteClipboardToFolder(folderId: number): Promise<OperationResult> {
  requireLocalAccess('Pasting clipboard files')
  return call<OperationResult>('gallery.pasteClipboardToFolder', { folderId })
}

// WebView2 DOM File 通过附加对象传输，文件内容不进入 JSON。
// 将拖入的文件对象交给后端导入指定图库文件夹。
export function importDroppedFilesToFolder(
  folderId: number,
  files: File[]
): Promise<OperationResult> {
  requireLocalAccess('Importing dropped files')
  return callWithAdditionalObjects<OperationResult>(
    'gallery.importDroppedFilesToFolder',
    { folderId },
    files,
    0
  )
}

export function deleteAssets(params: DeleteAssetsParams): Promise<DeleteAssetsResult> {
  return call<DeleteAssetsResult>('gallery.deleteAssets', params)
}

export function moveAssetsToFolder(params: MoveAssetsToFolderParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.moveAssetsToFolder', params)
}

export function checkAssetReachable(assetId: number): Promise<AssetReachability> {
  return call<AssetReachability>('gallery.checkAssetReachable', { assetId })
}

export function updateAssetsReviewState(
  params: UpdateAssetsReviewStateParams
): Promise<OperationResult> {
  return call<OperationResult>('gallery.updateAssetsReviewState', params)
}

export function getTimelineBuckets(
  params: GetTimelineBucketsParams = {}
): Promise<TimelineBucketsResponse> {
  return call<TimelineBucketsResponse>('gallery.getTimelineBuckets', params)
}

export function getAssetsByMonth(
  params: GetAssetsByMonthParams
): Promise<GetAssetsByMonthResponse> {
  return call<GetAssetsByMonthResponse>('gallery.getAssetsByMonth', params)
}

export function queryAssets(params: QueryAssetsParams): Promise<QueryAssetsResponse> {
  return call<QueryAssetsResponse>('gallery.queryAssets', params)
}

export function queryAssetLayoutMeta(
  params: QueryAssetLayoutMetaParams
): Promise<QueryAssetLayoutMetaResponse> {
  return call<QueryAssetLayoutMetaResponse>('gallery.queryAssetLayoutMeta', params)
}

export function getAssetMainColors(assetId: number): Promise<AssetMainColor[]> {
  return call<AssetMainColor[]>('gallery.getAssetMainColors', { assetId })
}

export function getTagTree(): Promise<TagTreeNode[]> {
  return call<TagTreeNode[]>('gallery.getTagTree', {})
}

export function listTags(): Promise<Tag[]> {
  return call<Tag[]>('gallery.listTags', {})
}

export async function createTag(params: CreateTagParams): Promise<{ id: number }> {
  const id = await call<number>('gallery.createTag', params)
  return { id }
}

export function updateTag(params: UpdateTagParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.updateTag', params)
}

export function deleteTag(tagId: number): Promise<OperationResult> {
  return call<OperationResult>('gallery.deleteTag', { id: tagId })
}

export function getTagStats(): Promise<TagStats[]> {
  return call<TagStats[]>('gallery.getTagStats', {})
}

export function getHomeStats(): Promise<HomeStats> {
  return call<HomeStats>('gallery.getHomeStats', {})
}

export function getBatchSelectionSummary(assetIds: number[]): Promise<BatchSelectionSummary> {
  return call<BatchSelectionSummary>('gallery.getBatchSelectionSummary', { assetIds })
}

export function addTagsToAsset(params: AddTagsToAssetParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.addTagsToAsset', params)
}

export function addTagToAssets(params: AddTagToAssetsParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.addTagToAssets', params)
}

export function removeTagFromAssets(params: RemoveTagFromAssetsParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.removeTagFromAssets', params)
}

export function removeTagsFromAsset(params: RemoveTagsFromAssetParams): Promise<OperationResult> {
  return call<OperationResult>('gallery.removeTagsFromAsset', params)
}

export function getAssetTags(assetId: number): Promise<Tag[]> {
  return call<Tag[]>('gallery.getAssetTags', { assetId })
}

export function updateAssetDescription(
  params: UpdateAssetDescriptionParams
): Promise<OperationResult> {
  return call<OperationResult>('gallery.updateAssetDescription', params)
}

export function updateAssetsDescription(
  params: UpdateAssetsDescriptionParams
): Promise<OperationResult> {
  return call<OperationResult>('gallery.updateAssetsDescription', params)
}

export function getTagsByAssetIds(assetIds: number[]): Promise<Record<number, Tag[]>> {
  return call<Record<number, Tag[]>>('gallery.getTagsByAssetIds', { assetIds })
}

export const galleryApi = {
  // 数据查询
  getFolderTree,
  createFolder,
  updateFolderDisplayName,
  openFolderInExplorer,
  removeFolderWatch,
  queryAssets,
  queryAssetLayoutMeta,
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
  getMissingAssets,
  purgeMissingAssets,

  // 标签管理
  getTagTree,
  listTags,
  createTag,
  updateTag,
  deleteTag,
  getTagStats,
  getHomeStats,
  getBatchSelectionSummary,

  // 资产-标签关联
  addTagsToAsset,
  addTagToAssets,
  removeTagFromAssets,
  removeTagsFromAsset,
  getAssetTags,
  getTagsByAssetIds,

  // URL 工具
  getAssetThumbnailUrl,
  getAssetUrl,

  // 资产动作
  openAssetDefault,
  revealAssetInExplorer,
  copyAssetsToClipboard,
  pasteClipboardToFolder,
  importDroppedFilesToFolder,
  deleteAssets,
  moveAssetsToFolder,
  checkAssetReachable,
  updateAssetsReviewState,
  updateAssetDescription,
  updateAssetsDescription,
}
