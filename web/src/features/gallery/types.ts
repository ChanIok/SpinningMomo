// Gallery模块类型定义 - Vue版本
// 基于 React 版本，去掉 React 特定的 Props 类型

// ============= 核心数据类型 =============

export interface Asset {
  id: number
  name: string
  path: string
  type: AssetType // photo, video, live_photo, unknown
  dominantColorHex?: string
  rating: number
  reviewFlag: ReviewFlag

  // 基本信息
  width?: number
  height?: number
  size?: number // 文件大小（字节）
  mimeType?: string
  hash?: string
  rootId?: number
  relativePath?: string
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

export type ReviewFlag = 'none' | 'picked' | 'rejected'

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

export type {
  // 资产动作/标签动作参数（RPC DTO）
  UpdateFolderDisplayNameParams,
  CreateTagParams,
  UpdateTagParams,
  AddTagsToAssetParams,
  RemoveTagsFromAssetParams,
  UpdateAssetsReviewStateParams,
  MoveAssetsToFolderParams,
  UpdateAssetDescriptionParams,
  SetInfinityNikkiUserRecordParams,
  SetInfinityNikkiWorldRecordParams,
  InfinityNikkiUserRecordCodeType,
  GetAssetTagsParams,
  // 统计/展示数据（仅由 API 返回）
  TagStats,
  HomeStats,
  // 颜色/关联数据
  AssetMainColor,
  AssetTag,
  // 扫描忽略规则（用于扫描对话框）
  IgnoreRule,
  ScanIgnoreRule,
} from './api/dto'

// ============= 视图配置类型 =============

// 视图模式
export type ViewMode = 'masonry' | 'grid' | 'list' | 'adaptive'

// 排序选项
export type SortBy = 'createdAt' | 'name' | 'size' | 'resolution'
export type SortOrder = 'asc' | 'desc'

// 筛选器
export interface AssetFilter {
  type?: AssetType // photo, video, live_photo, unknown
  searchQuery?: string
  folderId?: string
  rating?: number
  reviewFlag?: ReviewFlag
  tagIds?: number[]
  tagMatchMode?: 'any' | 'all'
  clothIds?: number[]
  clothMatchMode?: 'any' | 'all'
  colorHex?: string
  colorDistance?: number
}

// 视图配置
export interface ViewConfig {
  mode: ViewMode
  size: number // 缩略图目标尺寸（px）
}

export type {
  // 扫描/可达性
  OperationResult,
  ScanAssetsParams,
  ScanAssetsResult,
  StartScanAssetsResult,
  AssetReachability,
  // 查询（timeline / grid / adaptive 共用过滤器语义）
  QueryAssetsFilters,
  QueryAssetsParams,
  QueryAssetsResponse,
  AssetLayoutMetaItem,
  QueryAssetLayoutMetaParams,
  QueryAssetLayoutMetaResponse,
  AdaptiveLayoutRowItem,
  AdaptiveLayoutRow,
  QueryPhotoMapPointsParams,
  PhotoMapPoint,
  // Infinity Nikki 解析结果
  InfinityNikkiExtractedParams,
  InfinityNikkiUserRecord,
  InfinityNikkiMapArea,
  InfinityNikkiDetails,
  GetInfinityNikkiMetadataNamesParams,
  InfinityNikkiMetadataNames,
  // 时间线桶与月视图
  TimelineBucket,
  GetTimelineBucketsParams,
  TimelineBucketsResponse,
  GetAssetsByMonthParams,
  GetAssetsByMonthResponse,
} from './api/dto'

// ============= UI状态类型 =============

// 选择状态
export interface SelectionState {
  selectedIds: Set<number>
  anchorIndex?: number
  activeIndex?: number
  activeAssetId?: number
}

// Lightbox状态
export interface LightboxState {
  isOpen: boolean
  /** 关闭动画阶段：为 true 时仍可认为灯箱打开，但 gallery 层已开始淡入 */
  isClosing: boolean
  /** 沉浸模式：仅页面内 Teleport + 固定层铺满视口，不调用系统/浏览器全屏 */
  isImmersive: boolean
  showFilmstrip: boolean
  zoom: number
  fitMode: 'contain' | 'cover' | 'actual'
}

// 侧边栏状态
export interface SidebarState {
  isOpen: boolean
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
