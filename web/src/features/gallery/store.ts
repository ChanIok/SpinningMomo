import { defineStore } from 'pinia'
import { ref, reactive, computed } from 'vue'
import type {
  Asset,
  ViewConfig,
  AssetFilter,
  SelectionState,
  LightboxState,
  SidebarState,
  SortBy,
  SortOrder,
  TimelineBucket,
  FolderTreeNode,
  TagTreeNode,
  DetailsPanelFocus,
} from './types'

/**
 * Gallery Pinia Store
 *
 * 数据流设计:
 * - Store 是单一数据来源，组件应直接从这里读取状态
 * - Composable 只负责协调 API 调用和调用 Store Actions
 */
export const useGalleryStore = defineStore('gallery', () => {
  // ============= 数据状态 =============
  const isLoading = ref(false)
  const isInitialLoading = ref(false)
  const error = ref<string | null>(null)
  const totalCount = ref(0)
  const currentPage = ref(1)
  const hasNextPage = ref(false)

  // ============= 分页缓存状态（普通模式使用） =============
  const paginatedAssets = ref<Map<number, Asset[]>>(new Map()) // key: pageNumber
  const perPage = ref(100) // 每页数量

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
    mode: 'grid',
    size: 200, // 默认缩略图尺寸 200px（中等大小，对应slider约41%位置）
  })

  const filter = ref<AssetFilter>({})
  const sortBy = ref<SortBy>('createdAt')
  const sortOrder = ref<SortOrder>('desc')
  const includeSubfolders = ref(true) // 默认包含子文件夹

  // ============= 选择状态 =============
  const selection = reactive<SelectionState>({
    selectedIds: new Set<number>(),
    lastSelectedId: undefined,
  })

  // ============= Lightbox状态 =============
  const lightbox = reactive<LightboxState>({
    isOpen: false,
    currentIndex: 0,
    isFullscreen: false,
    showFilmstrip: true,
    zoom: 1.0,
    fitMode: 'contain',
  })

  // ============= UI状态 =============
  const sidebar = reactive<SidebarState>({
    isOpen: true,
    activeSection: 'all',
  })

  // 详情面板焦点状态
  const detailsPanel: DetailsPanelFocus = reactive<DetailsPanelFocus>({
    type: 'none',
  })

  const detailsOpen = ref(true)

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
  }

  /**
   * 清空分页缓存（切换筛选条件时调用）
   */
  function clearPaginatedAssets() {
    paginatedAssets.value.clear()
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

  // ============= 文件夹树操作 Actions =============

  function setFolders(newFolders: FolderTreeNode[]) {
    folders.value = newFolders
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
  }

  function setTagsLoading(loading: boolean) {
    tagsLoading.value = loading
  }

  function setTagsError(errorMessage: string | null) {
    tagsError.value = errorMessage
  }

  // ============= 视图操作 Actions =============

  function setViewConfig(config: Partial<ViewConfig>) {
    viewConfig.value = { ...viewConfig.value, ...config }
  }

  function setFilter(newFilter: Partial<AssetFilter>) {
    filter.value = { ...filter.value, ...newFilter }
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
      selection.lastSelectedId = id
    } else {
      selection.selectedIds.delete(id)
    }
  }

  function clearSelection() {
    selection.selectedIds.clear()
    selection.lastSelectedId = undefined
  }

  // ============= Lightbox操作 Actions =============

  /**
   * 打开 Lightbox
   * @param index - 要打开的资产的全局索引
   */
  function openLightbox(index: number) {
    lightbox.isOpen = true
    lightbox.currentIndex = index
  }

  function closeLightbox() {
    lightbox.isOpen = false
    lightbox.currentIndex = 0
    lightbox.zoom = 1.0
    lightbox.fitMode = 'contain'

    // 退出全屏（如果在全屏状态）
    if (lightbox.isFullscreen && document.fullscreenElement) {
      document.exitFullscreen()
      lightbox.isFullscreen = false
    }
  }

  function goToLightboxIndex(index: number) {
    if (lightbox.isOpen) {
      const validIndex = Math.max(0, Math.min(index, totalCount.value - 1))
      lightbox.currentIndex = validIndex
    }
  }

  function goToPreviousLightbox() {
    if (lightbox.isOpen && lightbox.currentIndex > 0) {
      lightbox.currentIndex = lightbox.currentIndex - 1
    }
  }

  function goToNextLightbox() {
    if (lightbox.isOpen && lightbox.currentIndex < totalCount.value - 1) {
      lightbox.currentIndex = lightbox.currentIndex + 1
    }
  }

  function toggleLightboxFullscreen() {
    lightbox.isFullscreen = !lightbox.isFullscreen
  }

  function toggleLightboxFilmstrip() {
    lightbox.showFilmstrip = !lightbox.showFilmstrip
  }

  function setLightboxZoom(zoom: number) {
    lightbox.zoom = Math.max(0.5, Math.min(5, zoom))
  }

  function setLightboxFitMode(mode: LightboxState['fitMode']) {
    lightbox.fitMode = mode
  }

  // ============= UI操作 Actions =============

  function setSidebarOpen(open: boolean) {
    sidebar.isOpen = open
  }

  function setSidebarActiveSection(activeSection: SidebarState['activeSection']) {
    sidebar.activeSection = activeSection
  }

  function setDetailsOpen(open: boolean) {
    detailsOpen.value = open
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

    // 清空时间线数据
    clearTimelineData()

    // 清空分页缓存
    clearPaginatedAssets()

    // 清空文件夹树数据
    folders.value = []
    foldersLoading.value = false
    foldersError.value = null

    // 清空标签树数据
    tags.value = []
    tagsLoading.value = false
    tagsError.value = null

    viewConfig.value = { mode: 'adaptive', size: 200 }
    filter.value = {}
    sortBy.value = 'createdAt'
    sortOrder.value = 'desc'
    includeSubfolders.value = true

    selection.selectedIds.clear()
    selection.lastSelectedId = undefined

    lightbox.isOpen = false
    lightbox.currentIndex = 0

    sidebar.isOpen = true
    sidebar.activeSection = 'all'
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

    // 分页缓存状态
    paginatedAssets,
    perPage,

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

    // 分页缓存 Actions
    setPerPage,
    getAssetsInRange,
    isPageLoaded,
    setPageAssets,
    clearPaginatedAssets,

    // 时间线 Actions
    setTimelineBuckets,
    setTimelineTotalCount,
    clearTimelineData,

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
    setSorting,
    setIncludeSubfolders,

    selectAsset,
    clearSelection,

    openLightbox,
    closeLightbox,
    goToLightboxIndex,
    goToPreviousLightbox,
    goToNextLightbox,
    toggleLightboxFullscreen,
    toggleLightboxFilmstrip,
    setLightboxZoom,
    setLightboxFitMode,

    setSidebarOpen,
    setSidebarActiveSection,
    setDetailsOpen,
    setDetailsFocus,
    clearDetailsFocus,

    reset,
  }
})
