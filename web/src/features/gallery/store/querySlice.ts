import { ref, reactive } from 'vue'
import type { Asset, TimelineBucket } from '../types'

/**
 * Query Slice
 *
 * 关注点:
 * - 面向“当前查询结果集”的状态，不关心具体交互（selection/lightbox）
 * - 提供分页缓存与时间线元数据，供虚拟列表和数据加载层复用
 */
export function createQuerySlice() {
  // 全局查询态：加载、错误、总量、当前页。
  const isLoading = ref(false)
  const isInitialLoading = ref(false)
  const error = ref<string | null>(null)
  const totalCount = ref(0)
  const currentPage = ref(1)
  const hasNextPage = ref(false)
  const isRefreshing = ref(false)
  const queryVersion = ref(0)

  // ============= 分页缓存状态（普通模式使用） =============
  // paginatedAssets: 只缓存已加载页，避免一次性加载全量资产。
  const paginatedAssets = ref<Map<number, Asset[]>>(new Map()) // key: pageNumber
  // 显式 version 用于触发依赖 Map 结构变化的更新（Map 原地改动不总能被外层感知）。
  const paginatedAssetsVersion = ref(0)
  const perPage = ref(100) // 每页数量
  // 可见区由虚拟列表回传，用于决定“优先加载哪些页”。
  const visibleRange = reactive<{
    startIndex?: number
    endIndex?: number
  }>({
    startIndex: undefined,
    endIndex: undefined,
  })

  // ============= 时间线数据状态 =============
  // buckets 仅保存月份元信息，不保存每月资产明细（明细仍走分页查询）。
  const timelineBuckets = ref<TimelineBucket[]>([])
  const timelineTotalCount = ref(0)

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
    // 版本号是并发请求裁决核心：后到的旧响应不会覆盖新查询。
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
      // 全局索引 -> 页号 + 页内索引
      const pageNum = Math.floor(i / perPage.value) + 1
      const indexInPage = i % perPage.value
      const page = paginatedAssets.value.get(pageNum)

      result.push(page?.[indexInPage] ?? null)
    }

    return result
  }

  function isPageLoaded(pageNum: number): boolean {
    return paginatedAssets.value.has(pageNum)
  }

  function setPageAssets(pageNum: number, pageAssets: Asset[]) {
    paginatedAssets.value.set(pageNum, pageAssets)
    paginatedAssetsVersion.value += 1
  }

  function replacePaginatedAssets(pages: Map<number, Asset[]>) {
    paginatedAssets.value = new Map(pages)
    paginatedAssetsVersion.value += 1
  }

  function clearPaginatedAssets() {
    // 这里不替换 ref 对象本身，保持引用稳定；通过 version 告知外部“缓存已失效”。
    paginatedAssets.value.clear()
    paginatedAssetsVersion.value += 1
  }

  function setVisibleRange(startIndex?: number, endIndex?: number) {
    visibleRange.startIndex = startIndex
    visibleRange.endIndex = endIndex
  }

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

  function resetQueryState() {
    // query slice 的 reset 只负责“查询域”，不触碰筛选与交互态。
    isLoading.value = false
    isInitialLoading.value = false
    error.value = null
    totalCount.value = 0
    currentPage.value = 1
    hasNextPage.value = false
    isRefreshing.value = false
    queryVersion.value = 0

    clearTimelineData()
    clearPaginatedAssets()
    setVisibleRange(undefined, undefined)
  }

  return {
    isLoading,
    isInitialLoading,
    error,
    totalCount,
    currentPage,
    hasNextPage,
    isRefreshing,
    queryVersion,
    paginatedAssets,
    paginatedAssetsVersion,
    perPage,
    visibleRange,
    timelineBuckets,
    timelineTotalCount,
    setLoading,
    setInitialLoading,
    setError,
    setPagination,
    beginQueryRefresh,
    finishQueryRefresh,
    isQueryVersionCurrent,
    setPerPage,
    getAssetsInRange,
    isPageLoaded,
    setPageAssets,
    replacePaginatedAssets,
    clearPaginatedAssets,
    setVisibleRange,
    setTimelineBuckets,
    setTimelineTotalCount,
    clearTimelineData,
    resetQueryState,
  }
}
