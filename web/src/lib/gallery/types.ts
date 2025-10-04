// Gallery模块类型定义 - 匹配后端C++结构（使用驼峰命名）
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

// 视图模式
export type ViewMode = 'masonry' | 'grid' | 'list' | 'adaptive'

// 排序选项
export type SortBy = 'createdAt' | 'name' | 'size'
export type SortOrder = 'asc' | 'desc'

// 筛选器
export interface AssetFilter {
  type?: AssetType // photo, video, live_photo, unknown
  searchQuery?: string
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

// ============= 文件夹相关类型 =============

// 文件夹查询参数
export interface ListFoldersParams {
  parentId?: number
  includeHidden?: boolean
}

// 文件夹查询响应
export interface ListFoldersResponse {
  items: Folder[]
  totalCount: number
}

// 创建文件夹参数
export interface CreateFolderParams {
  path: string
  parentId?: number
  name: string
  displayName?: string
}

// 更新文件夹参数
export interface UpdateFolderParams {
  id: number
  name?: string
  displayName?: string
  coverAssetId?: number
  sortOrder?: number
  isHidden?: boolean
}

// ============= 标签相关类型 =============

// 标签查询参数
export interface ListTagsParams {
  parentId?: number
}

// 标签查询响应
export interface ListTagsResponse {
  items: Tag[]
  totalCount: number
}

// 创建标签参数
export interface CreateTagParams {
  name: string
  parentId?: number
}

// 更新标签参数
export interface UpdateTagParams {
  id: number
  name?: string
  parentId?: number
  sortOrder?: number
}

// 资产标签关联参数
export interface AssetTagParams {
  assetId: number
  tagIds: number[]
}

// ============= 忽略规则相关类型 =============

// 忽略规则查询参数
export interface ListIgnoreRulesParams {
  folderId?: number
  isEnabled?: boolean
}

// 忽略规则查询响应
export interface ListIgnoreRulesResponse {
  items: IgnoreRule[]
  totalCount: number
}

// 创建忽略规则参数
export interface CreateIgnoreRuleParams {
  folderId?: number
  rulePattern: string
  patternType: 'glob' | 'regex'
  ruleType: 'exclude' | 'include'
  description?: string
}

// 更新忽略规则参数
export interface UpdateIgnoreRuleParams {
  id: number
  rulePattern?: string
  patternType?: 'glob' | 'regex'
  ruleType?: 'exclude' | 'include'
  isEnabled?: boolean
  description?: string
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

// 组件Props类型
export interface AssetsGridProps {
  assets: Asset[]
  selectedIds: Set<number>
  activeId?: number
  viewConfig: ViewConfig
  onAssetClick: (asset: Asset) => void
  onAssetDoubleClick: (asset: Asset) => void
  onAssetSelect: (assetId: number, selected: boolean) => void
  onAssetContextMenu?: (asset: Asset, event: React.MouseEvent) => void
}

export interface AssetCardProps {
  asset: Asset
  isSelected: boolean
  isActive: boolean
  size: number
  onClick: (asset: Asset) => void
  onDoubleClick: (asset: Asset) => void
  onSelect: (assetId: number, selected: boolean) => void
  onContextMenu?: (asset: Asset, event: React.MouseEvent) => void
}

// 错误类型
export interface AssetError {
  code: string
  message: string
  details?: unknown
}
