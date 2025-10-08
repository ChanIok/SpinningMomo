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

  // 时间信息（统一使用时间戳）
  fileCreatedAt?: number
  fileModifiedAt?: number
  createdAt: number
  updatedAt: number
  deletedAt?: number
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

// 标签类型
export interface Tag {
  id: number
  name: string
  parentId?: number
  sortOrder: number
  createdAt: number
  updatedAt: number
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
  tagId?: string
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
  filterType?: AssetType
  searchQuery?: string
}

// 列表查询响应
export interface ListAssetsResponse {
  items: Asset[]
  totalCount: number
  currentPage: number
  perPage: number
  totalPages: number
}

// 获取单个资产参数
export interface GetAssetParams {
  id: number
}

// 删除资产参数
export interface DeleteAssetParams {
  id: number
  deleteFile?: boolean
}

// 操作结果
export interface OperationResult {
  success: boolean
  message: string
  affectedCount?: number
}

// 资产统计
export interface AssetStats {
  totalCount: number
  photoCount: number
  videoCount: number
  livePhotoCount: number
  totalSize: number
  oldestItemDate: string
  newestItemDate: string
}

// 扫描参数
export interface ScanAssetsParams {
  directories: string[]
  recursive?: boolean
  generateThumbnails?: boolean
  thumbnailMaxWidth?: number
  thumbnailMaxHeight?: number
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

// ============= UI状态类型 =============

// 选择状态
export interface SelectionState {
  selectedIds: Set<number>
  activeId?: number
  lastSelectedId?: number
}

// Lightbox状态
export interface LightboxState {
  isOpen: boolean
  currentIndex: number
  assets: Asset[]
}

// 侧边栏状态
export interface SidebarState {
  isOpen: boolean
  activeSection: 'all' | 'folders' | 'tags'
}

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
