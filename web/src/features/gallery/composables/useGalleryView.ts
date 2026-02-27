import { computed } from 'vue'
import { useGalleryStore } from '../store'
import type { ViewMode, SortBy, SortOrder, AssetFilter } from '../types'

/**
 * 非线性映射
 * 使用平方函数，让小尺寸调整更细腻，大尺寸跳跃更大
 */
function sliderToSize(position: number): number {
  const min = 100
  const max = 768
  const normalized = position / 100

  // 平方函数：前半段变化缓慢，后半段加速
  const squared = Math.pow(normalized, 2)
  const size = min + (max - min) * squared

  return Math.round(size)
}

/**
 * 反向映射
 */
function sizeToSlider(size: number): number {
  const min = 100
  const max = 768
  const normalized = (size - min) / (max - min)

  // 开平方（平方的逆运算）
  const position = Math.sqrt(Math.max(0, Math.min(1, normalized))) * 100

  return Math.round(position)
}

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

  // ============= 视图操作 =============

  /**
   * 设置视图模式
   */
  function setViewMode(mode: ViewMode) {
    store.setViewConfig({ mode })
    console.log('🎯 视图模式切换:', mode)
  }

  /**
   * 设置视图大小（从 slider 位置设置）
   * @param sliderPosition - Slider位置 (0-100)
   */
  function setViewSizeFromSlider(sliderPosition: number) {
    const size = sliderToSize(sliderPosition)
    const validSize = Math.max(100, Math.min(768, size))
    store.setViewConfig({ size: validSize })
    console.log('📏 视图大小调整:', validSize, 'px (slider:', sliderPosition, '%)')
  }

  /**
   * 直接设置视图大小（从实际px值设置）
   * @param size - 实际尼寸 (100-768px)
   */
  function setViewSize(size: number) {
    const validSize = Math.max(100, Math.min(768, size))
    store.setViewConfig({ size: validSize })
    console.log('📏 视图大小调整:', validSize, 'px')
  }

  /**
   * 获取当前尺寸对应的 slider 位置
   */
  function getSliderPosition(): number {
    return sizeToSlider(viewSize.value)
  }

  /**
   * 增加视图大小（键盘快捷键）
   */
  function increaseSize() {
    const currentSlider = getSliderPosition()
    if (currentSlider < 100) {
      // 每次增加 5% slider 位置
      setViewSizeFromSlider(Math.min(100, currentSlider + 5))
    }
  }

  /**
   * 减少视图大小（键盘快捷键）
   */
  function decreaseSize() {
    const currentSlider = getSliderPosition()
    if (currentSlider > 0) {
      // 每次减少 5% slider 位置
      setViewSizeFromSlider(Math.max(0, currentSlider - 5))
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

    // 视图操作
    setViewMode,
    setViewSize,
    setViewSizeFromSlider,
    getSliderPosition,
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
