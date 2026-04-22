import type { Asset, AssetType, ReviewFlag, SortBy, SortOrder } from '../types'

// =========================
// Gallery API DTO (RPC types)
// =========================

// ============= 文件夹/标签/资产动作参数 =============

export interface UpdateFolderDisplayNameParams {
  id: number
  displayName: string
}

// 标签统计类型（仅 API 查询/展示用）
export interface TagStats {
  id: number
  name: string
  assetCount: number
}

export interface HomeStats {
  totalCount: number
  photoCount: number
  videoCount: number
  livePhotoCount: number
  totalSize: number
  todayAddedCount: number
}

// 创建标签参数
export interface CreateTagParams {
  name: string
  parentId?: number
  sortOrder?: number
}

// 更新标签参数
export interface UpdateTagParams {
  id: number
  name?: string
  parentId?: number
  sortOrder?: number
}

// 为资产添加标签参数
export interface AddTagsToAssetParams {
  assetId: number
  tagIds: number[]
}

export interface AddTagToAssetsParams {
  assetIds: number[]
  tagId: number
}

// 从资产移除标签参数
export interface RemoveTagsFromAssetParams {
  assetId: number
  tagIds: number[]
}

export interface UpdateAssetsReviewStateParams {
  assetIds: number[]
  rating?: number
  reviewFlag?: ReviewFlag
}

export interface MoveAssetsToFolderParams {
  ids: number[]
  targetFolderId: number
}

export interface UpdateAssetDescriptionParams {
  assetId: number
  description?: string
}

export type InfinityNikkiUserRecordCodeType = 'dye' | 'home_building'

export interface SetInfinityNikkiUserRecordParams {
  assetId: number
  codeType: InfinityNikkiUserRecordCodeType
  codeValue?: string
}

// 获取资产标签参数
export interface GetAssetTagsParams {
  assetId: number
}

// 资产标签关联类型
export interface AssetTag {
  assetId: number
  tagId: number
  createdAt: number
}

export interface AssetMainColor {
  r: number
  g: number
  b: number
  weight: number
}

// 忽略规则类型
export interface IgnoreRule {
  id: number
  folderId?: number
  rulePattern: string
  patternType: 'glob' | 'regex'
  ruleType: 'exclude' | 'include'
  isEnabled: boolean
  description?: string
  createdAt: number
  updatedAt: number
}

// 扫描时提交的忽略规则（对应后端 ScanIgnoreRule）
export interface ScanIgnoreRule {
  pattern: string
  patternType?: 'glob' | 'regex'
  ruleType?: 'exclude' | 'include'
  description?: string
}

// ============= API请求/响应类型 =============

// 查询响应数据（grid/list/masonry 时间线等共用）
export interface QueryAssetsResponseData {
  items: Asset[]
  totalCount: number
  currentPage: number
  perPage: number
  totalPages: number
  activeAssetIndex?: number
}

// 操作结果
export interface OperationResult {
  success: boolean
  message: string
  affectedCount?: number
  failedCount?: number
  notFoundCount?: number
  unchangedCount?: number
}

// 扫描参数
export interface ScanAssetsParams {
  directory: string
  generateThumbnails?: boolean
  thumbnailShortEdge?: number
  supportedExtensions?: string[]
  ignoreRules?: ScanIgnoreRule[]
  forceReanalyze?: boolean
  rebuildThumbnails?: boolean
}

// 扫描结果
export interface ScanAssetsResult {
  totalFiles: number
  newItems: number
  updatedItems: number
  deletedItems: number
  errors: string[]
  scanDuration: string
}

export interface StartScanAssetsResult {
  taskId: string
}

export interface AssetReachability {
  exists: boolean
  readable: boolean
  path?: string
  reason?: string
}

// ============= 统一查询相关类型 =============

// 查询过滤器
export interface QueryAssetsFilters {
  folderId?: number
  includeSubfolders?: boolean
  month?: string // "2024-10" 格式
  year?: string // "2024" 格式
  type?: AssetType // photo, video, live_photo
  search?: string // 搜索关键词
  rating?: number
  reviewFlag?: ReviewFlag
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
  clothIds?: number[]
  clothMatchMode?: 'any' | 'all'
  colorHexes?: string[]
  colorMatchMode?: 'any' | 'all'
  colorDistance?: number
}

// 查询参数
export interface QueryAssetsParams {
  filters: QueryAssetsFilters
  sortBy?: SortBy
  sortOrder?: SortOrder
  activeAssetId?: number
  // 分页是可选的：传page就分页，不传就返回所有结果
  page?: number
  perPage?: number
}

// 查询响应（grid/list/masonry 时间线等共用）
export type QueryAssetsResponse = QueryAssetsResponseData

export interface AssetLayoutMetaItem {
  id: number
  width?: number
  height?: number
}

export interface QueryAssetLayoutMetaParams {
  filters: QueryAssetsFilters
  sortBy?: SortBy
  sortOrder?: SortOrder
}

export interface QueryAssetLayoutMetaResponse {
  items: AssetLayoutMetaItem[]
  totalCount: number
}

export interface AdaptiveLayoutRowItem {
  index: number
  id: number
  width: number
  height: number
  aspectRatio: number
}

export interface AdaptiveLayoutRow {
  index: number
  start: number
  size: number
  items: AdaptiveLayoutRowItem[]
}

export interface QueryPhotoMapPointsParams {
  filters: QueryAssetsFilters
  sortBy?: SortBy
  sortOrder?: SortOrder
  worldId: string
}

export interface PhotoMapPoint {
  assetId: number
  name: string
  hash?: string
  fileCreatedAt?: number
  nikkiLocX: number
  nikkiLocY: number
  nikkiLocZ?: number
  assetIndex: number
}

export interface InfinityNikkiExtractedParams {
  cameraParams?: string
  timeHour?: number
  timeMin?: number
  cameraFocalLength?: number
  rotation?: number
  apertureValue?: number
  filterId?: number
  filterStrength?: number
  vignetteIntensity?: number
  lightId?: number
  lightStrength?: number
  vertical?: number
  bloomIntensity?: number
  bloomThreshold?: number
  brightness?: number
  exposure?: number
  contrast?: number
  saturation?: number
  vibrance?: number
  highlights?: number
  shadow?: number
  nikkiLocX?: number
  nikkiLocY?: number
  nikkiLocZ?: number
  nikkiHidden?: number
  poseId?: number
}

export interface InfinityNikkiUserRecord {
  codeType: InfinityNikkiUserRecordCodeType
  codeValue: string
}

export interface InfinityNikkiDetails {
  extracted?: InfinityNikkiExtractedParams
  userRecord?: InfinityNikkiUserRecord
}

export interface GetInfinityNikkiMetadataNamesParams {
  filterId?: number
  poseId?: number
  lightId?: number
  locale?: 'zh-CN' | 'en-US'
}

export interface InfinityNikkiMetadataNames {
  filterName?: string
  poseName?: string
  lightName?: string
}

// ============= 时间线相关类型 =============

export interface TimelineBucket {
  month: string // "2024-10" 格式
  count: number // 该月照片数量
}

export interface GetTimelineBucketsParams {
  folderId?: number
  includeSubfolders?: boolean
  sortOrder?: 'asc' | 'desc'
  activeAssetId?: number
  type?: AssetType
  search?: string
  rating?: number
  reviewFlag?: ReviewFlag
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
  clothIds?: number[]
  clothMatchMode?: 'any' | 'all'
  colorHexes?: string[]
  colorMatchMode?: 'any' | 'all'
  colorDistance?: number
}

export interface TimelineBucketsResponse {
  buckets: TimelineBucket[]
  totalCount: number
  activeAssetIndex?: number
}

export interface GetAssetsByMonthParams {
  month: string // "2024-10" 格式
  folderId?: number
  includeSubfolders?: boolean
  sortOrder?: 'asc' | 'desc'
  type?: AssetType
  search?: string
  rating?: number
  reviewFlag?: ReviewFlag
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
  clothIds?: number[]
  clothMatchMode?: 'any' | 'all'
  colorHexes?: string[]
  colorMatchMode?: 'any' | 'all'
  colorDistance?: number
}

export interface GetAssetsByMonthResponse {
  month: string
  assets: Asset[]
  count: number
}
