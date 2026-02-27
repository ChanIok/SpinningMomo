// Gallery模块类型定义 - Vue版本
// 基于 React 版本，去掉 React 特定的 Props 类型

// ============= 核心数据类型 =============

export interface Asset {
  id: number
  name: string
  path: string
  type: AssetType // photo, video, live_photo, unknown

  // 基本信息
  width?: number
  height?: number
  size?: number // 文件大小（字节）
  mimeType?: string
  hash?: string
  folderId?: number
  description?: string
  extension?: string

  // 时间信息（统一使用时间戳）
  fileCreatedAt?: number
  fileModifiedAt?: number
  createdAt: number
  updatedAt: number
}

// 资产类型枚举
export type AssetType = 'photo' | 'video' | 'live_photo' | 'unknown'

// 文件夹类型
export interface Folder {
  id: number
  path: string
  parentId?: number
  name: string
  displayName?: string
  coverAssetId?: number
  sortOrder: number
  isHidden: boolean
  createdAt: number
  updatedAt: number
}

// 文件夹树节点类型（用于侧边栏导航）
export interface FolderTreeNode {
  id: number
  path: string
  parentId?: number
  name: string
  displayName?: string
  coverAssetId?: number
  sortOrder: number
  isHidden: boolean
  createdAt: number
  updatedAt: number
  assetCount: number // 包含所有子文件夹的 assets 总数
  children: FolderTreeNode[]
}

// 标签类型
export interface Tag {
  id: number
  name: string
  parentId?: number
  sortOrder: number
  createdAt: number
  updatedAt: number
}

// 标签树节点类型（用于侧边栏导航）
export interface TagTreeNode {
  id: number
  name: string
  parentId?: number
  sortOrder: number
  createdAt: number
  updatedAt: number
  assetCount: number // 包含所有子标签的 assets 总数
  children: TagTreeNode[]
}

// 标签统计类型
export interface TagStats {
  id: number
  name: string
  assetCount: number
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

// 从资产移除标签参数
export interface RemoveTagsFromAssetParams {
  assetId: number
  tagIds: number[]
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

// ============= 视图配置类型 =============

// 视图模式
export type ViewMode = 'masonry' | 'grid' | 'list' | 'adaptive'

// 排序选项
export type SortBy = 'createdAt' | 'name' | 'size'
export type SortOrder = 'asc' | 'desc'

// 筛选器
export interface AssetFilter {
  type?: AssetType // photo, video, live_photo, unknown
  searchQuery?: string
  folderId?: string
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
}

// 视图配置
export interface ViewConfig {
  mode: ViewMode
  size: number // 缩略图大小，1-5的等级
}

// ============= API请求/响应类型 =============

// 列表查询参数
export interface ListAssetsParams {
  page?: number
  perPage?: number
  sortBy?: SortBy
  sortOrder?: SortOrder
  // 文件夹筛选参数（可选）
  folderId?: number
  includeSubfolders?: boolean
}

// 列表查询响应
export interface ListAssetsResponse {
  items: Asset[]
  totalCount: number
  currentPage: number
  perPage: number
  totalPages: number
}

// 操作结果
export interface OperationResult {
  success: boolean
  message: string
  affectedCount?: number
}

// 扫描参数
export interface ScanAssetsParams {
  directory: string
  generateThumbnails?: boolean
  thumbnailShortEdge?: number
  supportedExtensions?: string[]
  ignoreRules?: ScanIgnoreRule[]
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

// ============= 统一查询相关类型 =============

// 查询过滤器
export interface QueryAssetsFilters {
  folderId?: number
  includeSubfolders?: boolean
  month?: string // "2024-10" 格式
  year?: string // "2024" 格式
  type?: AssetType // photo, video, live_photo
  search?: string // 搜索关键词
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
}

// 查询参数
export interface QueryAssetsParams {
  filters: QueryAssetsFilters
  sortBy?: SortBy
  sortOrder?: SortOrder
  // 分页是可选的：传page就分页，不传就返回所有结果
  page?: number
  perPage?: number
}

// 查询响应（复用 ListAssetsResponse）
export type QueryAssetsResponse = ListAssetsResponse

// ============= 时间线相关类型 =============

// 时间线桶（月份元数据）
export interface TimelineBucket {
  month: string // "2024-10" 格式
  count: number // 该月照片数量
}

// 获取时间线桶参数
export interface GetTimelineBucketsParams {
  folderId?: number
  includeSubfolders?: boolean
  type?: AssetType
  search?: string
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
}

// 获取时间线桶响应
export interface TimelineBucketsResponse {
  buckets: TimelineBucket[]
  totalCount: number
}

// 获取指定月份资产参数
export interface GetAssetsByMonthParams {
  month: string // "2024-10" 格式
  folderId?: number
  includeSubfolders?: boolean
  sortOrder?: 'asc' | 'desc'
}

// 获取指定月份资产响应
export interface GetAssetsByMonthResponse {
  month: string
  assets: Asset[]
  count: number
}

// ============= UI状态类型 =============

// 选择状态
export interface SelectionState {
  selectedIds: Set<number>
  lastSelectedId?: number
}

// Lightbox状态
export interface LightboxState {
  isOpen: boolean
  currentIndex: number
  isFullscreen: boolean
  showFilmstrip: boolean
  zoom: number
  fitMode: 'contain' | 'cover' | 'actual'
}

// 侧边栏状态
export interface SidebarState {
  isOpen: boolean
  activeSection: 'all' | 'folders' | 'tags'
}

// 详情面板焦点状态
export type DetailsPanelFocus =
  | { type: 'none' }
  | { type: 'folder'; folder: FolderTreeNode }
  | { type: 'tag'; tag: TagTreeNode }
  | { type: 'asset'; asset: Asset }
  | { type: 'batch' }

// ============= 错误类型 =============

export interface AssetError {
  code: string
  message: string
  details?: unknown
}

// 自定义错误类
export class GalleryError extends Error {
  code: string
  details?: unknown

  constructor(code: string, message: string, details?: unknown) {
    super(message)
    this.name = 'GalleryError'
    this.code = code
    this.details = details
  }
}
