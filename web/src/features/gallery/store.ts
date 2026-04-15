import { defineStore } from 'pinia'
import { ref, reactive, computed } from 'vue'
import { useStorage } from '@vueuse/core'
import type {
  Asset,
  ViewConfig,
  AssetFilter,
  SelectionState,
  LightboxState,
  SidebarState,
  SortBy,
  SortOrder,
  ViewMode,
  TimelineBucket,
  FolderTreeNode,
  TagTreeNode,
  DetailsPanelFocus,
} from './types'

const GALLERY_VIEW_SIZE_STORAGE_KEY = 'spinningmomo.gallery.view.size'
const GALLERY_VIEW_MODE_STORAGE_KEY = 'spinningmomo.gallery.view.mode'
// 侧边栏树的展开状态是纯前端视图信息，只需落在 localStorage。
const GALLERY_EXPANDED_FOLDERS_STORAGE_KEY = 'spinningmomo.gallery.sidebar.expanded-folders'
const GALLERY_EXPANDED_TAGS_STORAGE_KEY = 'spinningmomo.gallery.sidebar.expanded-tags'
const LIGHTBOX_MIN_ZOOM = 0.05
const LIGHTBOX_MAX_ZOOM = 5

function normalizeGalleryViewSize(size: number): number {
  const numericSize = Number(size)
  return Number.isNaN(numericSize) ? 128 : numericSize
}

function normalizeGalleryViewMode(mode: unknown): ViewMode {
  return mode === 'masonry' || mode === 'list' || mode === 'adaptive' || mode === 'grid'
    ? mode
    : 'grid'
}

function normalizeExpandedIds(ids: unknown): number[] {
  if (!Array.isArray(ids)) {
    return []
  }

  // localStorage 可能被旧值或手工调试污染，进入运行态前统一清洗。
  return [...new Set(ids.map((id) => Number(id)).filter((id) => Number.isInteger(id)))]
}

function collectTreeIds<T extends { id: number; children: T[] }>(nodes: T[]): number[] {
  const ids: number[] = []

  for (const node of nodes) {
    ids.push(node.id)
    ids.push(...collectTreeIds(node.children))
  }

  return ids
}

/**
 * Gallery Pinia Store
 *
 * 数据流设计:
 * - Store 是单一数据来源，组件应直接从这里读取状态
 * - Composable 只负责协调 API 调用和调用 Store Actions
 */
export const useGalleryStore = defineStore('gallery', () => {
  const persistedViewSize = useStorage<number>(GALLERY_VIEW_SIZE_STORAGE_KEY, 128)
  persistedViewSize.value = normalizeGalleryViewSize(persistedViewSize.value)
  const persistedViewMode = useStorage<ViewMode>(GALLERY_VIEW_MODE_STORAGE_KEY, 'grid')
  persistedViewMode.value = normalizeGalleryViewMode(persistedViewMode.value)
  const persistedExpandedFolderIds = useStorage<number[]>(GALLERY_EXPANDED_FOLDERS_STORAGE_KEY, [])
  persistedExpandedFolderIds.value = normalizeExpandedIds(persistedExpandedFolderIds.value)
  const persistedExpandedTagIds = useStorage<number[]>(GALLERY_EXPANDED_TAGS_STORAGE_KEY, [])
  persistedExpandedTagIds.value = normalizeExpandedIds(persistedExpandedTagIds.value)

  // ============= 数据状态 =============
  const isLoading = ref(false)
  const isInitialLoading = ref(false)
  const error = ref<string | null>(null)
  const totalCount = ref(0)
  const currentPage = ref(1)
  const hasNextPage = ref(false)
  const isRefreshing = ref(false)
  const queryVersion = ref(0)

  // ============= 分页缓存状态（普通模式使用） =============
  const paginatedAssets = ref<Map<number, Asset[]>>(new Map()) // key: pageNumber
  const paginatedAssetsVersion = ref(0)
  const perPage = ref(100) // 每页数量
  const visibleRange = reactive<{
    startIndex?: number
    endIndex?: number
  }>({
    startIndex: undefined,
    endIndex: undefined,
  })

  // ============= 时间线数据状态 =============
  const timelineBuckets = ref<TimelineBucket[]>([])
  const timelineTotalCount = ref(0)

  // ============= 文件夹树状态 =============
  const folders = ref<FolderTreeNode[]>([])
  const foldersLoading = ref(false)
  const foldersError = ref<string | null>(null)

  // ============= 标签树状态 =============
  const tags = ref<TagTreeNode[]>([])
  const tagsLoading = ref(false)
  const tagsError = ref<string | null>(null)

  // ============= 视图配置 =============
  const viewConfig = ref<ViewConfig>({
    mode: persistedViewMode.value,
    size: persistedViewSize.value,
  })

  const filter = ref<AssetFilter>({})
  const sortBy = ref<SortBy>('createdAt')
  const sortOrder = ref<SortOrder>('desc')
  const includeSubfolders = ref(true) // 默认包含子文件夹

  // ============= 选择状态 =============
  const selection = reactive<SelectionState>({
    selectedIds: new Set<number>(),
    anchorIndex: undefined,
    // activeIndex 是当前结果集里的位置缓存；筛选/排序变化后可能失效，需要重定位。
    activeIndex: undefined,
    // activeAssetId 才是“当前聚焦资产”的身份真相源，用来跨结果集变化保留语义。
    activeAssetId: undefined,
  })

  // ============= Lightbox状态 =============
  const lightbox = reactive<LightboxState>({
    isOpen: false,
    isClosing: false,
    isImmersive: false,
    showFilmstrip: true,
    zoom: 1.0,
    fitMode: 'contain',
  })

  const sidebar = reactive<SidebarState>({
    isOpen: true,
  })

  // 详情面板焦点状态
  const detailsPanel: DetailsPanelFocus = reactive<DetailsPanelFocus>({
    type: 'none',
  })

  const detailsOpen = ref(true)
  const pendingOpenAssetId = ref<number | undefined>(undefined)

  // ============= 计算属性 =============
  const selectedCount = computed(() => selection.selectedIds.size)
  const hasSelection = computed(() => selectedCount.value > 0)
  const isTimelineMode = computed(() => sortBy.value === 'createdAt')

  // 文件夹树根节点资产总数（所有根节点的 assetCount 之和）
  const foldersAssetTotalCount = computed(() => {
    return folders.value.reduce((sum, folder) => sum + folder.assetCount, 0)
  })

  // 标签树根节点资产总数（所有根节点的 assetCount 之和）
  const tagsAssetTotalCount = computed(() => {
    return tags.value.reduce((sum, tag) => sum + tag.assetCount, 0)
  })

  // 持久化层使用数组，读取层转成 Set，兼顾存储简单与查询高频。
  const expandedFolderIdSet = computed(() => new Set(persistedExpandedFolderIds.value))
  const expandedTagIdSet = computed(() => new Set(persistedExpandedTagIds.value))

  // ============= 状态操作 Actions =============

  function setLoading(loading: boolean) {
    isLoading.value = loading
  }

  function setInitialLoading(loading: boolean) {
    isInitialLoading.value = loading
  }

  function setError(errorMessage: string | null) {
    error.value = errorMessage
  }

  function setPagination(total: number, page: number, hasNext: boolean) {
    totalCount.value = total
    currentPage.value = page
    hasNextPage.value = hasNext
  }

  function beginQueryRefresh(): number {
    queryVersion.value += 1
    isRefreshing.value = true
    return queryVersion.value
  }

  function finishQueryRefresh(version: number) {
    if (queryVersion.value === version) {
      isRefreshing.value = false
    }
  }

  function isQueryVersionCurrent(version: number): boolean {
    return queryVersion.value === version
  }

  // ============= 分页缓存操作 Actions =============

  function setPerPage(count: number) {
    perPage.value = count
  }

  /**
   * 获取指定索引范围的资产（用于虚拟列表）
   * @returns Asset[] | null[] - null 表示该位置数据未加载
   */
  function getAssetsInRange(startIndex: number, endIndex: number): (Asset | null)[] {
    const result: (Asset | null)[] = []

    for (let i = startIndex; i <= endIndex; i++) {
      const pageNum = Math.floor(i / perPage.value) + 1
      const indexInPage = i % perPage.value
      const page = paginatedAssets.value.get(pageNum)

      result.push(page?.[indexInPage] ?? null)
    }

    return result
  }

  /**
   * 检查某个页是否已加载
   */
  function isPageLoaded(pageNum: number): boolean {
    return paginatedAssets.value.has(pageNum)
  }

  /**
   * 设置某页数据
   */
  function setPageAssets(pageNum: number, pageAssets: Asset[]) {
    paginatedAssets.value.set(pageNum, pageAssets)
    paginatedAssetsVersion.value += 1
  }

  function replacePaginatedAssets(pages: Map<number, Asset[]>) {
    paginatedAssets.value = new Map(pages)
    paginatedAssetsVersion.value += 1
  }

  function patchAssetsReviewState(
    assetIds: number[],
    updates: Partial<Pick<Asset, 'rating' | 'reviewFlag'>>
  ) {
    if (assetIds.length === 0) {
      return
    }

    const assetIdSet = new Set(assetIds)

    // 审片操作是高频交互，这里直接原地 patch 当前已加载页面，避免每次按键都整页重载。
    paginatedAssets.value.forEach((pageAssets, pageNum) => {
      let hasPageChange = false
      const nextPageAssets = pageAssets.map((asset) => {
        if (!assetIdSet.has(asset.id)) {
          return asset
        }

        hasPageChange = true
        return {
          ...asset,
          ...(updates.rating !== undefined ? { rating: updates.rating } : {}),
          ...(updates.reviewFlag !== undefined ? { reviewFlag: updates.reviewFlag } : {}),
        }
      })

      if (hasPageChange) {
        paginatedAssets.value.set(pageNum, nextPageAssets)
        paginatedAssetsVersion.value += 1
      }
    })

    if (detailsPanel.type === 'asset' && assetIdSet.has(detailsPanel.asset.id)) {
      detailsPanel.asset = {
        ...detailsPanel.asset,
        ...(updates.rating !== undefined ? { rating: updates.rating } : {}),
        ...(updates.reviewFlag !== undefined ? { reviewFlag: updates.reviewFlag } : {}),
      }
    }
  }

  function patchAssetDescription(assetId: number, description?: string) {
    paginatedAssets.value.forEach((pageAssets, pageNum) => {
      let hasPageChange = false
      const nextPageAssets = pageAssets.map((asset) => {
        if (asset.id !== assetId) {
          return asset
        }

        hasPageChange = true
        return {
          ...asset,
          description,
        }
      })

      if (hasPageChange) {
        paginatedAssets.value.set(pageNum, nextPageAssets)
        paginatedAssetsVersion.value += 1
      }
    })

    if (detailsPanel.type === 'asset' && detailsPanel.asset.id === assetId) {
      detailsPanel.asset = {
        ...detailsPanel.asset,
        description,
      }
    }
  }

  /**
   * 清空分页缓存（切换筛选条件时调用）
   */
  function clearPaginatedAssets() {
    paginatedAssets.value.clear()
    paginatedAssetsVersion.value += 1
  }

  function setVisibleRange(startIndex?: number, endIndex?: number) {
    visibleRange.startIndex = startIndex
    visibleRange.endIndex = endIndex
  }

  // ============= 时间线数据操作 Actions =============

  function setTimelineBuckets(buckets: TimelineBucket[]) {
    timelineBuckets.value = buckets
  }

  function setTimelineTotalCount(count: number) {
    timelineTotalCount.value = count
  }

  function clearTimelineData() {
    timelineBuckets.value = []
    timelineTotalCount.value = 0
  }

  // 这里只持久化“哪些节点被展开”，不把整棵树的 UI 快照塞进本地存储。
  function setFolderExpanded(folderId: number, expanded: boolean) {
    const nextExpandedIds = new Set(persistedExpandedFolderIds.value)

    if (expanded) {
      nextExpandedIds.add(folderId)
    } else {
      nextExpandedIds.delete(folderId)
    }

    persistedExpandedFolderIds.value = [...nextExpandedIds]
  }

  function toggleFolderExpanded(folderId: number) {
    setFolderExpanded(folderId, !isFolderExpanded(folderId))
  }

  function isFolderExpanded(folderId: number): boolean {
    return expandedFolderIdSet.value.has(folderId)
  }

  function setTagExpanded(tagId: number, expanded: boolean) {
    const nextExpandedIds = new Set(persistedExpandedTagIds.value)

    if (expanded) {
      nextExpandedIds.add(tagId)
    } else {
      nextExpandedIds.delete(tagId)
    }

    persistedExpandedTagIds.value = [...nextExpandedIds]
  }

  function toggleTagExpanded(tagId: number) {
    setTagExpanded(tagId, !isTagExpanded(tagId))
  }

  function isTagExpanded(tagId: number): boolean {
    return expandedTagIdSet.value.has(tagId)
  }

  // ============= 文件夹树操作 Actions =============

  function setFolders(newFolders: FolderTreeNode[]) {
    folders.value = newFolders

    // 树重载后把已不存在的节点 id 裁掉，避免 localStorage 越积越脏。
    const validFolderIds = new Set(collectTreeIds(newFolders))
    persistedExpandedFolderIds.value = persistedExpandedFolderIds.value.filter((id) =>
      validFolderIds.has(id)
    )
  }

  function setFoldersLoading(loading: boolean) {
    foldersLoading.value = loading
  }

  function setFoldersError(errorMessage: string | null) {
    foldersError.value = errorMessage
  }

  // ============= 标签树操作 Actions =============

  function setTags(newTags: TagTreeNode[]) {
    tags.value = newTags

    // 标签树和文件夹树一样，刷新后同步清理失效展开状态。
    const validTagIds = new Set(collectTreeIds(newTags))
    persistedExpandedTagIds.value = persistedExpandedTagIds.value.filter((id) =>
      validTagIds.has(id)
    )
  }

  function setTagsLoading(loading: boolean) {
    tagsLoading.value = loading
  }

  function setTagsError(errorMessage: string | null) {
    tagsError.value = errorMessage
  }

  // ============= 视图操作 Actions =============

  function setViewConfig(config: Partial<ViewConfig>) {
    const merged = { ...viewConfig.value, ...config }
    merged.size = normalizeGalleryViewSize(merged.size)
    merged.mode = normalizeGalleryViewMode(merged.mode)
    viewConfig.value = merged
    persistedViewSize.value = viewConfig.value.size
    persistedViewMode.value = viewConfig.value.mode
  }

  function setFilter(newFilter: Partial<AssetFilter>) {
    filter.value = { ...filter.value, ...newFilter }
  }

  function resetFilter() {
    filter.value = {}
  }

  function setSorting(newSortBy: SortBy, newSortOrder: SortOrder) {
    sortBy.value = newSortBy
    sortOrder.value = newSortOrder
  }

  function setIncludeSubfolders(include: boolean) {
    includeSubfolders.value = include
  }

  // ============= 选择操作 Actions =============

  function selectAsset(id: number, selected: boolean, multi = false) {
    if (!multi) {
      selection.selectedIds.clear()
    }

    if (selected) {
      selection.selectedIds.add(id)
    } else {
      selection.selectedIds.delete(id)
    }
  }

  function clearSelection() {
    selection.selectedIds.clear()
    selection.anchorIndex = undefined
  }

  function replaceSelection(ids: number[]) {
    selection.selectedIds.clear()
    ids.forEach((id) => selection.selectedIds.add(id))
  }

  function setSelectionAnchor(index?: number) {
    selection.anchorIndex = index
  }

  function setSelectionActive(index?: number) {
    selection.activeIndex = index
  }

  function setActiveAsset(assetId: number, index?: number) {
    selection.activeAssetId = assetId
    selection.activeIndex = index
  }

  function setActiveAssetId(assetId?: number) {
    selection.activeAssetId = assetId
  }

  function clearActiveAsset() {
    selection.activeAssetId = undefined
    selection.activeIndex = undefined
  }

  // ============= Lightbox操作 Actions =============

  /**
   * 打开 Lightbox
   * @param index - 要打开的资产的全局索引
   */
  function resetLightboxView() {
    lightbox.zoom = 1.0
    lightbox.fitMode = 'contain'
  }

  function openLightbox() {
    resetLightboxView()
    lightbox.isClosing = false
    lightbox.isOpen = true
  }

  function setLightboxClosing(closing: boolean) {
    lightbox.isClosing = closing
  }

  function closeLightbox() {
    lightbox.isOpen = false
    lightbox.isClosing = false
    lightbox.isImmersive = false
    resetLightboxView()
  }

  function goToLightboxIndex(index: number) {
    if (lightbox.isOpen) {
      const validIndex = Math.max(0, Math.min(index, totalCount.value - 1))
      selection.activeIndex = validIndex
    }
  }

  function goToPreviousLightbox() {
    const currentIndex = selection.activeIndex ?? 0
    if (lightbox.isOpen && currentIndex > 0) {
      selection.activeIndex = currentIndex - 1
    }
  }

  function goToNextLightbox() {
    const currentIndex = selection.activeIndex ?? 0
    if (lightbox.isOpen && currentIndex < totalCount.value - 1) {
      selection.activeIndex = currentIndex + 1
    }
  }

  function setLightboxImmersive(immersive: boolean) {
    lightbox.isImmersive = immersive
  }

  function toggleLightboxImmersive() {
    setLightboxImmersive(!lightbox.isImmersive)
  }

  function toggleLightboxFilmstrip() {
    lightbox.showFilmstrip = !lightbox.showFilmstrip
  }

  function setLightboxZoom(zoom: number) {
    lightbox.zoom = Math.max(LIGHTBOX_MIN_ZOOM, Math.min(LIGHTBOX_MAX_ZOOM, zoom))
  }

  function setLightboxFitMode(mode: LightboxState['fitMode']) {
    lightbox.fitMode = mode
  }

  // ============= UI操作 Actions =============

  function setSidebarOpen(open: boolean) {
    sidebar.isOpen = open
  }

  function setDetailsOpen(open: boolean) {
    detailsOpen.value = open
  }

  function setPendingOpenAssetId(assetId?: number) {
    pendingOpenAssetId.value = assetId
  }

  function consumePendingOpenAssetId(): number | undefined {
    const assetId = pendingOpenAssetId.value
    pendingOpenAssetId.value = undefined
    return assetId
  }

  // 详情面板焦点操作
  function setDetailsFocus(focus: DetailsPanelFocus) {
    Object.assign(detailsPanel, focus)
  }

  function clearDetailsFocus() {
    detailsPanel.type = 'none'
  }

  // ============= 重置操作 =============

  function reset() {
    paginatedAssets.value.clear()
    isLoading.value = false
    isInitialLoading.value = false
    error.value = null
    totalCount.value = 0
    currentPage.value = 1
    hasNextPage.value = false
    isRefreshing.value = false
    queryVersion.value = 0

    // 清空时间线数据
    clearTimelineData()

    // 清空分页缓存
    clearPaginatedAssets()
    setVisibleRange(undefined, undefined)

    // 清空文件夹树数据
    folders.value = []
    foldersLoading.value = false
    foldersError.value = null

    // 清空标签树数据
    tags.value = []
    tagsLoading.value = false
    tagsError.value = null

    persistedExpandedFolderIds.value = []
    persistedExpandedTagIds.value = []

    viewConfig.value = { mode: 'grid', size: 128 }
    persistedViewSize.value = viewConfig.value.size
    persistedViewMode.value = viewConfig.value.mode
    resetFilter()
    sortBy.value = 'createdAt'
    sortOrder.value = 'desc'
    includeSubfolders.value = true

    selection.selectedIds.clear()
    selection.anchorIndex = undefined
    selection.activeIndex = undefined
    selection.activeAssetId = undefined

    lightbox.isOpen = false
    lightbox.isClosing = false
    lightbox.isImmersive = false
    pendingOpenAssetId.value = undefined

    sidebar.isOpen = true
    detailsOpen.value = true
  }

  return {
    // 状态
    isLoading,
    isInitialLoading,
    error,
    totalCount,
    currentPage,
    hasNextPage,
    isRefreshing,
    queryVersion,

    // 分页缓存状态
    paginatedAssets,
    paginatedAssetsVersion,
    perPage,
    visibleRange,

    // 时间线状态
    timelineBuckets,
    timelineTotalCount,

    // 文件夹树状态
    folders,
    foldersLoading,
    foldersError,

    // 标签树状态
    tags,
    tagsLoading,
    tagsError,

    viewConfig,
    filter,
    sortBy,
    sortOrder,
    includeSubfolders,

    selection,
    lightbox,
    sidebar,
    detailsPanel,
    detailsOpen,
    pendingOpenAssetId,

    // 计算属性
    selectedCount,
    hasSelection,
    isTimelineMode,
    foldersAssetTotalCount,
    tagsAssetTotalCount,

    setLoading,
    setInitialLoading,
    setError,
    setPagination,
    beginQueryRefresh,
    finishQueryRefresh,
    isQueryVersionCurrent,

    // 分页缓存 Actions
    setPerPage,
    getAssetsInRange,
    isPageLoaded,
    setPageAssets,
    replacePaginatedAssets,
    patchAssetsReviewState,
    patchAssetDescription,
    clearPaginatedAssets,
    setVisibleRange,

    // 时间线 Actions
    setTimelineBuckets,
    setTimelineTotalCount,
    clearTimelineData,

    setFolderExpanded,
    toggleFolderExpanded,
    isFolderExpanded,
    setTagExpanded,
    toggleTagExpanded,
    isTagExpanded,

    // 文件夹树 Actions
    setFolders,
    setFoldersLoading,
    setFoldersError,

    // 标签树 Actions
    setTags,
    setTagsLoading,
    setTagsError,

    setViewConfig,
    setFilter,
    resetFilter,
    setSorting,
    setIncludeSubfolders,

    selectAsset,
    clearSelection,
    replaceSelection,
    setSelectionAnchor,
    setSelectionActive,
    setActiveAsset,
    setActiveAssetId,
    clearActiveAsset,

    resetLightboxView,
    openLightbox,
    setLightboxClosing,
    closeLightbox,
    goToLightboxIndex,
    goToPreviousLightbox,
    goToNextLightbox,
    setLightboxImmersive,
    toggleLightboxImmersive,
    toggleLightboxFilmstrip,
    setLightboxZoom,
    setLightboxFitMode,

    setSidebarOpen,
    setDetailsOpen,
    setPendingOpenAssetId,
    consumePendingOpenAssetId,
    setDetailsFocus,
    clearDetailsFocus,

    reset,
  }
})
