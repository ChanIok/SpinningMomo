import { computed } from 'vue'
import { useGalleryStore } from '../store'
import type { ViewMode, SortBy, SortOrder, AssetFilter } from '../types'

/**
 * Gallery视图管理 Composable
 * 负责视图模式切换、排序、筛选等视图相关逻辑
 */
export function useGalleryView() {
  const store = useGalleryStore()

  // ============= 视图状态 =============
  const viewConfig = computed(() => store.viewConfig)
  const viewMode = computed(() => store.viewConfig.mode)
  const viewSize = computed(() => store.viewConfig.size)
  const filter = computed(() => store.filter)
  const sortBy = computed(() => store.sortBy)
  const sortOrder = computed(() => store.sortOrder)
  const includeSubfolders = computed(() => store.includeSubfolders)

  // ============= 计算属性 =============

  /**
   * 根据视图大小计算列数
   */
  const columnCount = computed(() => {
    const size = viewSize.value
    switch (size) {
      case 1:
        return 2 // 最小
      case 2:
        return 3 // 小
      case 3:
        return 4 // 中等（默认）
      case 4:
        return 5 // 大
      case 5:
        return 6 // 最大
      default:
        return 4
    }
  })

  /**
   * 缩略图尺寸（像素）
   */
  const thumbnailSize = computed(() => {
    const size = viewSize.value
    switch (size) {
      case 1:
        return 120 // 最小
      case 2:
        return 160 // 小
      case 3:
        return 200 // 中等（默认）
      case 4:
        return 240 // 大
      case 5:
        return 280 // 最大
      default:
        return 200
    }
  })

  /**
   * 筛选后的资产列表
   */
  const filteredAssets = computed(() => {
    let result = store.assets

    // 按类型筛选
    if (filter.value.type) {
      result = result.filter((asset) => asset.type === filter.value.type)
    }

    // 按搜索关键词筛选
    if (filter.value.searchQuery) {
      const query = filter.value.searchQuery.toLowerCase()
      result = result.filter(
        (asset) =>
          asset.name.toLowerCase().includes(query) || asset.path.toLowerCase().includes(query)
      )
    }

    return result
  })

  /**
   * 排序后的资产列表
   */
  const sortedAssets = computed(() => {
    const result = [...filteredAssets.value]

    result.sort((a, b) => {
      let aValue: any
      let bValue: any

      switch (sortBy.value) {
        case 'name':
          aValue = a.name.toLowerCase()
          bValue = b.name.toLowerCase()
          break
        case 'size':
          aValue = a.size || 0
          bValue = b.size || 0
          break
        case 'createdAt':
        default:
          aValue = a.createdAt
          bValue = b.createdAt
          break
      }

      if (aValue < bValue) {
        return sortOrder.value === 'asc' ? -1 : 1
      }
      if (aValue > bValue) {
        return sortOrder.value === 'asc' ? 1 : -1
      }
      return 0
    })

    return result
  })

  // ============= 视图操作 =============

  /**
   * 设置视图模式
   */
  function setViewMode(mode: ViewMode) {
    store.setViewConfig({ mode })
    console.log('🎯 视图模式切换:', mode)
  }

  /**
   * 设置视图大小
   */
  function setViewSize(size: number) {
    const validSize = Math.max(1, Math.min(5, size))
    store.setViewConfig({ size: validSize })
    console.log('📏 视图大小调整:', validSize)
  }

  /**
   * 增加视图大小
   */
  function increaseSize() {
    if (viewSize.value < 5) {
      setViewSize(viewSize.value + 1)
    }
  }

  /**
   * 减少视图大小
   */
  function decreaseSize() {
    if (viewSize.value > 1) {
      setViewSize(viewSize.value - 1)
    }
  }

  /**
   * 设置排序
   */
  function setSorting(newSortBy: SortBy, newSortOrder: SortOrder) {
    store.setSorting(newSortBy, newSortOrder)
    console.log('🔄 排序设置:', { sortBy: newSortBy, sortOrder: newSortOrder })
  }

  /**
   * 切换排序方向
   */
  function toggleSortOrder() {
    const newOrder = sortOrder.value === 'asc' ? 'desc' : 'asc'
    setSorting(sortBy.value, newOrder)
  }

  /**
   * 设置筛选条件
   */
  function setFilter(newFilter: Partial<AssetFilter>) {
    store.setFilter(newFilter)
    console.log('🔍 筛选条件更新:', newFilter)
  }

  /**
   * 清空筛选条件
   */
  function clearFilter() {
    store.setFilter({})
    console.log('🧹 筛选条件已清空')
  }

  /**
   * 设置搜索关键词
   */
  function setSearchQuery(query: string) {
    setFilter({ searchQuery: query.trim() || undefined })
  }

  /**
   * 设置类型筛选
   */
  function setTypeFilter(type: AssetFilter['type']) {
    setFilter({ type })
  }

  /**
   * 设置是否包含子文件夹
   */
  function setIncludeSubfolders(include: boolean) {
    store.setIncludeSubfolders(include)
    console.log('📁 包含子文件夹设置:', include)
  }

  // ============= 视图模式预设 =============

  /**
   * 网格视图预设
   */
  function setGridView() {
    setViewMode('grid')
  }

  /**
   * 瀑布流视图预设
   */
  function setMasonryView() {
    setViewMode('masonry')
  }

  /**
   * 列表视图预设
   */
  function setListView() {
    setViewMode('list')
  }

  /**
   * 自适应视图预设
   */
  function setAdaptiveView() {
    setViewMode('adaptive')
  }

  return {
    // 状态
    viewConfig,
    viewMode,
    viewSize,
    filter,
    sortBy,
    sortOrder,
    includeSubfolders,

    // 计算属性
    columnCount,
    thumbnailSize,
    filteredAssets,
    sortedAssets,

    // 视图操作
    setViewMode,
    setViewSize,
    increaseSize,
    decreaseSize,

    // 排序操作
    setSorting,
    toggleSortOrder,

    // 筛选操作
    setFilter,
    clearFilter,
    setSearchQuery,
    setTypeFilter,
    setIncludeSubfolders,

    // 视图模式预设
    setGridView,
    setMasonryView,
    setListView,
    setAdaptiveView,
  }
}
