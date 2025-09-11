// Assets模块类型定义 - 基于后端API结构
export interface Asset {
  id: number
  filename: string
  filepath: string
  relative_path: string
  type: 'photo' | 'video' | 'live_photo' | 'unknown'

  // 基本信息
  width?: number
  height?: number
  file_size?: number
  mime_type: string
  file_hash?: string

  // 时间信息
  created_at: string
  updated_at: string
  deleted_at?: string
}

// 视图模式
export type ViewMode = 'masonry' | 'grid' | 'list' | 'adaptive'

// 排序选项
export type SortBy = 'created_at' | 'filename' | 'file_size'
export type SortOrder = 'asc' | 'desc'

// 筛选器
export interface AssetFilter {
  type?: string // photo, video, live_photo
  search_query?: string
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
  per_page?: number
  sort_by?: SortBy
  sort_order?: SortOrder
  filter_type?: string
  search_query?: string
}

// 列表查询响应
export interface ListAssetsResponse {
  items: Asset[]
  total_count: number
  current_page: number
  per_page: number
  total_pages: number
}

// 获取单个资产参数
export interface GetAssetParams {
  id: number
}

// 删除资产参数
export interface DeleteAssetParams {
  id: number
  delete_file?: boolean
}

// 操作结果
export interface OperationResult {
  success: boolean
  message: string
  affected_count?: number
}

// 资产统计
export interface AssetStats {
  total_count: number
  photo_count: number
  video_count: number
  live_photo_count: number
  total_size: number
  oldest_item_date: string
  newest_item_date: string
}

// 扫描参数
export interface ScanAssetsParams {
  directories: string[]
  recursive?: boolean
  generate_thumbnails?: boolean
  thumbnail_max_width?: number
  thumbnail_max_height?: number
}

// 扫描结果
export interface ScanAssetsResult {
  total_files: number
  new_items: number
  updated_items: number
  deleted_items: number
  errors: string[]
  scan_duration: string
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
