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
} from './types'

/**
 * Gallery Pinia Store
 */
export const useGalleryStore = defineStore('gallery', () => {
  // ============= 数据状态 =============
  const assets = ref<Asset[]>([])
  const isLoading = ref(false)
  const isInitialLoading = ref(false)
  const error = ref<string | null>(null)
  const totalCount = ref(0)
  const currentPage = ref(1)
  const hasNextPage = ref(false)

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
    activeId: undefined,
    lastSelectedId: undefined,
  })

  // ============= Lightbox状态 =============
  const lightbox = reactive<LightboxState>({
    isOpen: false,
    currentIndex: 0,
    assets: [],
  })

  // ============= UI状态 =============
  const sidebar = reactive<SidebarState>({
    isOpen: true,
    activeSection: 'all',
  })

  const detailsOpen = ref(true)

  // ============= 计算属性 =============
  const selectedCount = computed(() => selection.selectedIds.size)
  const hasSelection = computed(() => selectedCount.value > 0)
  const isAllSelected = computed(
    () => assets.value.length > 0 && selectedCount.value === assets.value.length
  )

  // ============= 数据操作 Actions =============

  function setAssets(newAssets: Asset[]) {
    assets.value = newAssets
  }

  function addAssets(newAssets: Asset[]) {
    const existingIds = new Set(assets.value.map((a) => a.id))
    const uniqueAssets = newAssets.filter((a) => !existingIds.has(a.id))
    assets.value = [...assets.value, ...uniqueAssets]
  }

  function updateAsset(id: number, updates: Partial<Asset>) {
    const index = assets.value.findIndex((asset) => asset.id === id)
    if (index !== -1 && assets.value[index]) {
      // 直接更新属性，避免类型推导问题
      Object.assign(assets.value[index], updates)
    }
  }

  function removeAsset(id: number) {
    // 移除资产
    assets.value = assets.value.filter((a) => a.id !== id)

    // 清理选择状态
    selection.selectedIds.delete(id)
    if (selection.activeId === id) {
      selection.activeId = undefined
    }
    if (selection.lastSelectedId === id) {
      selection.lastSelectedId = undefined
    }

    // 更新lightbox
    if (lightbox.isOpen) {
      lightbox.assets = lightbox.assets.filter((a) => a.id !== id)
      if (lightbox.currentIndex >= lightbox.assets.length) {
        lightbox.currentIndex = Math.max(0, lightbox.assets.length - 1)
      }
      if (lightbox.assets.length === 0) {
        lightbox.isOpen = false
      }
    }
  }

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

  function selectAll() {
    selection.selectedIds.clear()
    assets.value.forEach((asset) => {
      selection.selectedIds.add(asset.id)
    })
  }

  function clearSelection() {
    selection.selectedIds.clear()
    selection.lastSelectedId = undefined
  }

  function setActiveAsset(activeId?: number) {
    selection.activeId = activeId
  }

  // ============= Lightbox操作 Actions =============

  function openLightbox(lightboxAssets: Asset[], startIndex: number) {
    const validIndex = Math.max(0, Math.min(startIndex, lightboxAssets.length - 1))
    lightbox.isOpen = true
    lightbox.assets = lightboxAssets
    lightbox.currentIndex = validIndex
  }

  function closeLightbox() {
    lightbox.isOpen = false
    lightbox.currentIndex = 0
    lightbox.assets = []
  }

  function goToLightboxIndex(index: number) {
    if (lightbox.isOpen && lightbox.assets.length > 0) {
      const validIndex = Math.max(0, Math.min(index, lightbox.assets.length - 1))
      lightbox.currentIndex = validIndex
    }
  }

  function goToPreviousLightbox() {
    if (lightbox.isOpen && lightbox.currentIndex > 0) {
      lightbox.currentIndex = lightbox.currentIndex - 1
    }
  }

  function goToNextLightbox() {
    if (lightbox.isOpen && lightbox.currentIndex < lightbox.assets.length - 1) {
      lightbox.currentIndex = lightbox.currentIndex + 1
    }
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

  // ============= 重置操作 =============

  function reset() {
    assets.value = []
    isLoading.value = false
    isInitialLoading.value = false
    error.value = null
    totalCount.value = 0
    currentPage.value = 1
    hasNextPage.value = false

    viewConfig.value = { mode: 'adaptive', size: 200 }
    filter.value = {}
    sortBy.value = 'createdAt'
    sortOrder.value = 'desc'
    includeSubfolders.value = true

    selection.selectedIds.clear()
    selection.activeId = undefined
    selection.lastSelectedId = undefined

    lightbox.isOpen = false
    lightbox.currentIndex = 0
    lightbox.assets = []

    sidebar.isOpen = true
    sidebar.activeSection = 'all'
    detailsOpen.value = true
  }

  return {
    // 状态
    assets,
    isLoading,
    isInitialLoading,
    error,
    totalCount,
    currentPage,
    hasNextPage,

    viewConfig,
    filter,
    sortBy,
    sortOrder,
    includeSubfolders,

    selection,
    lightbox,
    sidebar,
    detailsOpen,

    // 计算属性
    selectedCount,
    hasSelection,
    isAllSelected,

    // Actions
    setAssets,
    addAssets,
    updateAsset,
    removeAsset,

    setLoading,
    setInitialLoading,
    setError,
    setPagination,

    setViewConfig,
    setFilter,
    setSorting,
    setIncludeSubfolders,

    selectAsset,
    selectAll,
    clearSelection,
    setActiveAsset,

    openLightbox,
    closeLightbox,
    goToLightboxIndex,
    goToPreviousLightbox,
    goToNextLightbox,

    setSidebarOpen,
    setSidebarActiveSection,
    setDetailsOpen,

    reset,
  }
})
