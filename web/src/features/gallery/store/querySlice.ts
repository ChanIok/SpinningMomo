import { ref, reactive, shallowRef } from 'vue'
import type { Asset, Tag, TimelineBucket } from '../types'

export type GalleryQueryStatus = 'idle' | 'loading' | 'refreshing' | 'error'

/**
 * Query Slice
 *
 * 关注点:
 * - 面向“当前查询结果集”的状态，不关心具体交互（selection/lightbox）
 * - 提供分页缓存与时间线元数据，供虚拟列表和数据加载层复用
 */
export function createQuerySlice() {
  // 全局查询态：状态、错误、总量、当前页。
  const queryStatus = ref<GalleryQueryStatus>('idle')
  const error = ref<string | null>(null)
  const totalCount = ref(0)
  const currentPage = ref(1)
  const hasNextPage = ref(false)
  const queryVersion = ref(0)
  const dyeCodeAssetIds = ref<Set<number>>(new Set())
  const assetTagsById = ref<Map<number, Tag[]>>(new Map())
  const loadedAssetTagIds = ref<Set<number>>(new Set())
  // 资产标签缓存失效和详情面板刷新共用一个版本号，避免两个“标签变更信号”分叉。
  const assetTagsVersion = ref(0)

  // ============= 分页缓存状态（普通模式使用） =============
  // paginatedAssets: 只缓存已加载页，避免一次性加载全量资产。
  const paginatedAssets = shallowRef<Map<number, Asset[]>>(new Map()) // key: pageNumber
  // 显式 version 用于触发依赖 Map 结构变化的更新（Map 原地改动不总能被外层感知）。
  const paginatedAssetsVersion = ref(0)
  const perPage = ref(500) // 每页数量
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

  function setError(errorMessage: string | null) {
    error.value = errorMessage
    if (errorMessage !== null) {
      queryStatus.value = 'error'
    }
  }

  function setPagination(total: number, page: number, hasNext: boolean) {
    totalCount.value = total
    currentPage.value = page
    hasNextPage.value = hasNext
  }

  function beginQueryRefresh(): number {
    // 版本号是并发请求裁决核心：后到的旧响应不会覆盖新查询。
    queryVersion.value += 1
    const hasExistingResults =
      paginatedAssets.value.size > 0 || totalCount.value > 0 || timelineBuckets.value.length > 0
    queryStatus.value = hasExistingResults ? 'refreshing' : 'loading'
    return queryVersion.value
  }

  function finishQueryRefresh(version: number) {
    if (queryVersion.value === version && queryStatus.value !== 'error') {
      queryStatus.value = 'idle'
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
    const nextPages = new Map(paginatedAssets.value)
    nextPages.set(pageNum, pageAssets)
    paginatedAssets.value = nextPages
    paginatedAssetsVersion.value += 1
  }

  function setDyeCodeStatuses(queriedAssetIds: number[], matchingAssetIds: number[]) {
    const next = new Set(dyeCodeAssetIds.value)
    for (const assetId of queriedAssetIds) {
      next.delete(assetId)
    }
    for (const assetId of matchingAssetIds) {
      next.add(assetId)
    }
    dyeCodeAssetIds.value = next
  }

  function clearDyeCodeStatuses() {
    dyeCodeAssetIds.value = new Set()
  }

  function setAssetTagsForAssets(assetIds: number[], tagsByAssetId: Record<number, Tag[]>) {
    const nextTagsById = new Map(assetTagsById.value)
    const nextLoadedIds = new Set(loadedAssetTagIds.value)

    for (const assetId of assetIds) {
      nextTagsById.set(assetId, tagsByAssetId[assetId] ?? [])
      nextLoadedIds.add(assetId)
    }

    assetTagsById.value = nextTagsById
    loadedAssetTagIds.value = nextLoadedIds
  }

  function invalidateAssetTags(assetIds?: number[]) {
    if (assetIds === undefined) {
      assetTagsById.value = new Map()
      loadedAssetTagIds.value = new Set()
      assetTagsVersion.value += 1
      return
    }

    const nextTagsById = new Map(assetTagsById.value)
    const nextLoadedIds = new Set(loadedAssetTagIds.value)
    for (const assetId of assetIds) {
      nextTagsById.delete(assetId)
      nextLoadedIds.delete(assetId)
    }

    assetTagsById.value = nextTagsById
    loadedAssetTagIds.value = nextLoadedIds
    assetTagsVersion.value += 1
  }

  function replacePaginatedAssets(pages: Map<number, Asset[]>) {
    paginatedAssets.value = new Map(pages)
    clearDyeCodeStatuses()
    paginatedAssetsVersion.value += 1
  }

  function clearPaginatedAssets() {
    // shallowRef 只跟踪顶层引用；清空缓存直接替换 Map，语义更明确。
    paginatedAssets.value = new Map()
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
    queryStatus.value = 'idle'
    error.value = null
    totalCount.value = 0
    currentPage.value = 1
    hasNextPage.value = false
    queryVersion.value = 0

    clearTimelineData()
    clearPaginatedAssets()
    clearDyeCodeStatuses()
    invalidateAssetTags()
    setVisibleRange(undefined, undefined)
  }

  return {
    queryStatus,
    error,
    totalCount,
    currentPage,
    hasNextPage,
    queryVersion,
    dyeCodeAssetIds,
    assetTagsById,
    loadedAssetTagIds,
    assetTagsVersion,
    paginatedAssets,
    paginatedAssetsVersion,
    perPage,
    visibleRange,
    timelineBuckets,
    timelineTotalCount,
    setError,
    setPagination,
    beginQueryRefresh,
    finishQueryRefresh,
    isQueryVersionCurrent,
    setPerPage,
    getAssetsInRange,
    isPageLoaded,
    setPageAssets,
    setDyeCodeStatuses,
    clearDyeCodeStatuses,
    setAssetTagsForAssets,
    invalidateAssetTags,
    replacePaginatedAssets,
    clearPaginatedAssets,
    setVisibleRange,
    setTimelineBuckets,
    setTimelineTotalCount,
    clearTimelineData,
    resetQueryState,
  }
}
