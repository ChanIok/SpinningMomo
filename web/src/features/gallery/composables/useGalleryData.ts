import { useDebounceFn } from '@vueuse/core'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../store'
import { isGalleryLightboxOverlay, useGalleryOverlayHistory } from './useGalleryOverlayHistory'
import { runWithLayoutTransition } from './useGalleryLayoutTransition'
import { galleryApi } from '../api'
import type { Asset, AssetLayoutMetaItem, ScanAssetsParams } from '../types'
import { toQueryAssetsFilters } from '../queryFilters'
import { getDyeCodeAssetIds } from '@/extensions/infinity_nikki/api'

/** 模块级在途页码加载 Promise 映射，Key 为 `${queryVersion}:${pageNum}`，防止并发重复拉取并天然支持版本隔离 */
const inFlightPageLoads = new Map<string, Promise<void>>()

let notificationRefreshInFlight = false
let notificationRefreshQueued = false

const scheduleNotificationRefresh = useDebounceFn(async () => {
  if (notificationRefreshInFlight) {
    notificationRefreshQueued = true
    return
  }

  notificationRefreshInFlight = true
  do {
    notificationRefreshQueued = false
    try {
      const store = useGalleryStore()
      store.setFoldersError(null)
      const folderTree = await galleryApi.getFolderTree()
      store.setFolders(folderTree)

      const galleryData = useGalleryData()
      await galleryData.refreshCurrentQuery()
    } catch (error) {
      console.error('Failed to refresh gallery after notification:', error)
    }
  } while (notificationRefreshQueued)

  notificationRefreshInFlight = false
}, 400)

/**
 * Gallery数据管理 Composable
 * 负责协调 API 调用和 Store 操作
 * 组件应直接从 Store 读取状态，而不是通过这里的 computed 属性
 */
export function useGalleryData() {
  const store = useGalleryStore()
  const overlayHistory = useGalleryOverlayHistory()
  const { t } = useI18n()

  async function refreshDyeCodeStatuses(assets: Asset[], requestVersion: number) {
    const assetIds = [...new Set(assets.map((asset) => asset.id))]
    if (assetIds.length === 0) {
      return
    }

    try {
      const matchingAssetIds: number[] = []
      const batchSize = 800
      for (let start = 0; start < assetIds.length; start += batchSize) {
        const batch = assetIds.slice(start, start + batchSize)
        matchingAssetIds.push(...(await getDyeCodeAssetIds({ assetIds: batch })))
      }
      if (store.isQueryVersionCurrent(requestVersion)) {
        store.setDyeCodeStatuses(assetIds, matchingAssetIds)
      }
    } catch (error) {
      // 染色码角标是增强展示，失败不影响图库主体查询。
      console.warn('Failed to load Infinity Nikki dye code badges:', error)
    }
  }

  function findLoadedAssetById(assetId: number) {
    for (const pageAssets of store.paginatedAssets.values()) {
      const asset = pageAssets.find((item) => item.id === assetId)
      if (asset) {
        return asset
      }
    }

    return undefined
  }

  function getAnchorPageNumber() {
    const startIndex = store.visibleRange.startIndex
    if (startIndex === undefined || startIndex < 0) {
      return 1
    }

    return Math.floor(startIndex / store.perPage) + 1
  }

  function getVisiblePageNumbers(total: number): number[] {
    if (total <= 0) {
      return []
    }

    const maxIndex = total - 1
    const startIndex = store.visibleRange.startIndex
    const endIndex = store.visibleRange.endIndex

    if (startIndex === undefined || endIndex === undefined) {
      return [1]
    }

    const clampedStart = Math.max(0, Math.min(startIndex, maxIndex))
    const clampedEnd = Math.max(clampedStart, Math.min(endIndex, maxIndex))
    const startPage = Math.floor(clampedStart / store.perPage) + 1
    const endPage = Math.floor(clampedEnd / store.perPage) + 1
    const pages: number[] = []

    for (let pageNum = startPage; pageNum <= endPage; pageNum += 1) {
      pages.push(pageNum)
    }

    return pages.length > 0 ? pages : [1]
  }

  async function queryAssetPage(pageNum: number, activeAssetId?: number) {
    const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

    return galleryApi.queryAssets({
      filters,
      sortBy: store.sortBy,
      sortOrder: store.sortOrder,
      activeAssetId,
      page: pageNum,
      perPage: store.perPage,
    })
  }

  async function queryCurrentAssetIds() {
    const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)
    const response = await galleryApi.queryAssetLayoutMeta({
      filters,
      sortBy: store.sortBy,
      sortOrder: store.sortOrder,
    })

    return response.items.map((item) => item.id)
  }

  async function queryVisiblePages(
    total: number,
    preferredPage: number,
    preferredPageItems?: Asset[]
  ) {
    if (total <= 0) {
      return new Map<number, Asset[]>()
    }

    const maxPage = Math.max(1, Math.ceil(total / store.perPage))
    const visiblePages = new Set(getVisiblePageNumbers(total))
    const normalizedPreferredPage = Math.max(1, Math.min(preferredPage, maxPage))
    visiblePages.add(normalizedPreferredPage)
    const pageNumbers = [...visiblePages].sort((left, right) => left - right)

    const pages = new Map<number, Asset[]>()
    const pagesToFetch: number[] = []

    pageNumbers.forEach((pageNum) => {
      if (pageNum === normalizedPreferredPage && preferredPageItems) {
        pages.set(pageNum, preferredPageItems)
      } else {
        pagesToFetch.push(pageNum)
      }
    })

    if (pagesToFetch.length > 0) {
      const responses = await Promise.all(pagesToFetch.map((pageNum) => queryAssetPage(pageNum)))
      pagesToFetch.forEach((pageNum, index) => {
        pages.set(pageNum, responses[index]?.items ?? [])
      })
    }

    return pages
  }

  // 在筛选结果刷新后，用 activeAssetId 将当前位置重建到新的结果集上。
  async function reconcileActiveAsset(activeAssetIndex?: number, requestVersion?: number) {
    if (requestVersion !== undefined && !store.isQueryVersionCurrent(requestVersion)) {
      return
    }

    const activeAssetId = store.selection.activeAssetId
    if (activeAssetId === undefined) {
      return
    }

    // 资产详情/暗房是当前查询结果集的连续浏览器：当前资产消失时，落到同索引的新资产。
    if (activeAssetIndex === undefined) {
      const detailsTracksActiveAsset =
        store.detailsPanel.type === 'asset' && store.detailsPanel.assetId === activeAssetId
      const detailsTracksSelection = detailsTracksActiveAsset || store.detailsPanel.type === 'batch'
      const shouldRestoreAdjacentFocus = store.lightbox.isOpen || detailsTracksSelection

      if (shouldRestoreAdjacentFocus && store.totalCount > 0) {
        const targetIndex = Math.min(store.selection.activeIndex ?? 0, store.totalCount - 1)
        const targetPage = Math.floor(targetIndex / store.perPage) + 1
        if (!store.isPageLoaded(targetPage)) {
          await loadPage(targetPage)
          if (requestVersion !== undefined && !store.isQueryVersionCurrent(requestVersion)) {
            return
          }
        }

        const targetAsset = store.getAssetAt(targetIndex)
        if (targetAsset) {
          store.setActiveAsset(targetAsset.id, targetIndex)
          store.replaceSelection([targetAsset.id])
          store.setSelectionAnchor(targetIndex)
          store.setDetailsFocus({ type: 'asset', assetId: targetAsset.id })
          return
        }
      }

      if (store.lightbox.isOpen) {
        // 当前资产已从结果集消失，先关闭 URL 暗房层，让浏览器历史和 Store 同步收口。
        if (isGalleryLightboxOverlay(overlayHistory.snapshot.value.overlay)) {
          void overlayHistory.closeLightboxOverlay()
        } else {
          store.closeLightbox()
        }
      }

      if (detailsTracksSelection) {
        store.clearDetailsFocus()
      }

      store.clearSelection()
      store.clearActiveAsset()
      return
    }

    store.setSelectionActive(activeAssetIndex)

    // 重定位只返回索引；这里确保对应页面已加载，后续 UI 才能拿到完整资产对象。
    const targetPage = Math.floor(activeAssetIndex / store.perPage) + 1
    if (!store.isPageLoaded(targetPage)) {
      await loadPage(targetPage)
      if (requestVersion !== undefined && !store.isQueryVersionCurrent(requestVersion)) {
        return
      }
    }

    const loadedActiveAsset = findLoadedAssetById(activeAssetId)
    if (
      loadedActiveAsset &&
      store.detailsPanel.type === 'asset' &&
      store.detailsPanel.assetId === loadedActiveAsset.id
    ) {
      store.setDetailsFocus({ type: 'asset', assetId: loadedActiveAsset.id })
    }

    if (store.lightbox.isOpen && loadedActiveAsset) {
      store.replaceSelection([loadedActiveAsset.id])
      store.setSelectionAnchor(activeAssetIndex)
      store.setDetailsFocus({ type: 'asset', assetId: loadedActiveAsset.id })
    }
  }

  /**
   * 桌面端保留原有的资源管理器式初始聚焦；紧凑布局必须从未选择的浏览态开始。
   */
  function tryFocusFirstResultWhenDetailsEmpty(requestVersion: number) {
    if (store.isCompactWindow || !store.isQueryVersionCurrent(requestVersion)) {
      return
    }
    if (store.totalCount <= 0 || store.detailsPanel.type !== 'none') {
      return
    }
    if (store.selection.activeAssetId !== undefined || store.selectedCount > 0) {
      return
    }

    const firstAsset = store.paginatedAssets.get(1)?.[0]
    if (!firstAsset) {
      return
    }

    store.replaceSelection([firstAsset.id])
    store.setSelectionAnchor(0)
    store.setActiveAsset(firstAsset.id, 0)
    store.setDetailsFocus({ type: 'asset', assetId: firstAsset.id })
  }

  function getAssetFromPages(
    index: number,
    pages: Map<number, Asset[]>,
    pageSize: number
  ): Asset | null {
    if (!Number.isInteger(index) || index < 0 || pageSize <= 0) {
      return null
    }
    const pageNum = Math.floor(index / pageSize) + 1
    const indexInPage = index % pageSize
    return pages.get(pageNum)?.[indexInPage] ?? null
  }

  /**
   * 检查视口内真正可见的资产 ID 序列是否发生变化。
   * 无论整页总数是否有微调，只要用户肉眼可见区域内的卡片 ID 和排布 1:1 严格一致，
   * 界面在视觉上就是静止的，跳过 View Transition 过渡动画，避免呼吸灯闪烁。
   */
  function haveVisibleAssetsChanged(newPages: Map<number, Asset[]>): boolean {
    if (newPages.size === 0 && store.paginatedAssets.size === 0) {
      return false
    }
    if (newPages.size === 0 || store.paginatedAssets.size === 0) {
      return true
    }

    // 视口范围未知（虚拟器尚未上报或当前无可见卡片行）时，无法断言可见像素静止，
    // 按“已变化”处理以播放过渡；跳过动画只允许发生在范围确切且逐张比对一致时。
    const { startIndex, endIndex } = store.visibleRange
    if (startIndex === undefined || endIndex === undefined) {
      return true
    }

    for (let index = startIndex; index <= endIndex; index++) {
      const oldAsset = store.getAssetAt(index)
      const newAsset = getAssetFromPages(index, newPages, store.perPage)

      if (oldAsset?.id !== newAsset?.id) {
        return true
      }
    }

    return false
  }

  /**
   * 重新拉取当前查询条件下的布局元数据（宽高等轻量信息）。
   */
  async function reloadLayoutMeta(): Promise<AssetLayoutMetaItem[]> {
    const requestVersion = store.queryVersion
    const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

    try {
      const response = await galleryApi.queryAssetLayoutMeta({
        filters,
        sortBy: store.sortBy,
        sortOrder: store.sortOrder,
      })

      if (!store.isQueryVersionCurrent(requestVersion)) {
        return []
      }

      return response.items
    } catch (error) {
      console.error('Failed to load asset layout meta:', error)
      return []
    }
  }

  /**
   * 确保当前查询条件的布局元数据已就绪。
   * 切换至瀑布流或自适应布局前调用；若已有可用缓存则直接复用。
   */
  async function ensureLayoutMetaLoaded(): Promise<AssetLayoutMetaItem[]> {
    if (
      store.totalCount === 0 ||
      (store.layoutMetaItems.length === store.totalCount && store.totalCount > 0)
    ) {
      return store.layoutMetaItems
    }

    const items = await reloadLayoutMeta()
    if (items.length > 0) {
      store.setLayoutMetaItems(items)
    }
    return store.layoutMetaItems
  }

  /**
   * 非时间线模式：按当前筛选/排序拉取 `queryAssets` 分页结果并写入 store。
   * 网格、列表、瀑布流、自适应等视图共用，与布局无关。
   */
  async function refreshPagedAssetQuery() {
    const requestVersion = store.beginQueryRefresh()

    try {
      store.setError(null)

      let pageNum = getAnchorPageNumber()
      let response = await queryAssetPage(pageNum, store.selection.activeAssetId)

      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      const maxPage = Math.max(1, Math.ceil(response.totalCount / store.perPage))
      if (response.totalCount > 0 && pageNum > maxPage) {
        pageNum = maxPage
        response = await queryAssetPage(pageNum, store.selection.activeAssetId)
        if (!store.isQueryVersionCurrent(requestVersion)) {
          return
        }
      }

      const requiresLayoutMeta = store.view.mode === 'masonry' || store.view.mode === 'adaptive'

      const [pages, layoutMetaItems] = await Promise.all([
        queryVisiblePages(response.totalCount, pageNum, response.items),
        requiresLayoutMeta ? reloadLayoutMeta() : Promise.resolve(null),
      ])
      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      const applyUpdates = async () => {
        store.clearTimelineData()
        store.setPagination(response.totalCount, pageNum, pageNum < maxPage)
        if (layoutMetaItems) {
          store.setLayoutMetaItems(layoutMetaItems)
        } else if (!requiresLayoutMeta) {
          store.clearLayoutMetaItems()
        }
        store.replacePaginatedAssets(pages)
        await reconcileActiveAsset(response.activeAssetIndex, requestVersion)
        tryFocusFirstResultWhenDetailsEmpty(requestVersion)
      }

      if (haveVisibleAssetsChanged(pages)) {
        await runWithLayoutTransition(applyUpdates)
      } else {
        await applyUpdates()
      }
      void refreshDyeCodeStatuses([...pages.values()].flat(), requestVersion)

      console.log('📊 加载完成:', {
        totalCount: response.totalCount,
        loadedPages: [...pages.keys()],
        perPage: store.perPage,
      })
    } catch (error) {
      console.error('加载失败:', error)
      store.setError(t('gallery.query.loadFailed'))
    } finally {
      store.finishQueryRefresh(requestVersion)
    }
  }

  async function refreshTimelineData() {
    const requestVersion = store.beginQueryRefresh()

    try {
      store.setError(null)

      const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)
      const bucketsResponse = await galleryApi.getTimelineBuckets({
        folderId: filters.folderId,
        includeSubfolders: filters.includeSubfolders,
        sortOrder: store.sortOrder,
        activeAssetId: store.selection.activeAssetId,
        createdAtFrom: filters.createdAtFrom,
        createdAtTo: filters.createdAtTo,
        type: filters.type,
        shape: filters.shape,
        search: filters.search,
        ratings: filters.ratings,
        reviewFlag: filters.reviewFlag,
        tagIds: filters.tagIds,
        tagMatchMode: filters.tagMatchMode,
        colorHexes: filters.colorHexes,
        colorMatchMode: filters.colorMatchMode,
        colorDistance: filters.colorDistance,
      })

      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      const pageNum = Math.max(
        1,
        Math.min(
          getAnchorPageNumber(),
          Math.max(1, Math.ceil(bucketsResponse.totalCount / store.perPage))
        )
      )
      const requiresLayoutMeta = store.view.mode === 'masonry' || store.view.mode === 'adaptive'

      const [pages, layoutMetaItems] = await Promise.all([
        queryVisiblePages(bucketsResponse.totalCount, pageNum),
        requiresLayoutMeta ? reloadLayoutMeta() : Promise.resolve(null),
      ])
      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      const applyUpdates = async () => {
        store.setTimelineBuckets(bucketsResponse.buckets)
        store.setTimelineTotalCount(bucketsResponse.totalCount)
        store.setPagination(
          bucketsResponse.totalCount,
          pageNum,
          pageNum < Math.max(1, Math.ceil(bucketsResponse.totalCount / store.perPage))
        )
        if (layoutMetaItems) {
          store.setLayoutMetaItems(layoutMetaItems)
        } else if (!requiresLayoutMeta) {
          store.clearLayoutMetaItems()
        }
        store.replacePaginatedAssets(pages)
        await reconcileActiveAsset(bucketsResponse.activeAssetIndex, requestVersion)
        tryFocusFirstResultWhenDetailsEmpty(requestVersion)
      }

      if (haveVisibleAssetsChanged(pages)) {
        await runWithLayoutTransition(applyUpdates)
      } else {
        await applyUpdates()
      }
      void refreshDyeCodeStatuses([...pages.values()].flat(), requestVersion)

      console.log('📅 时间线数据加载成功:', {
        days: bucketsResponse.buckets.length,
        total: bucketsResponse.totalCount,
        loadedPages: [...pages.keys()],
      })
    } catch (error) {
      console.error('Failed to load timeline data:', error)
      store.setError(t('gallery.timeline.loadFailed'))
    } finally {
      store.finishQueryRefresh(requestVersion)
    }
  }

  // ============= 数据加载操作 =============

  /**
   * 加载时间线数据（日期元数据 + 当前可见页）
   */
  async function loadTimelineData() {
    await refreshTimelineData()
  }

  /**
   * 加载普通模式资产 - 保留旧结果，等新结果就绪后原子替换
   */
  async function loadAllAssets() {
    await refreshPagedAssetQuery()
  }

  async function refreshCurrentQuery() {
    if (store.isTimelineMode) {
      await refreshTimelineData()
      return
    }

    await refreshPagedAssetQuery()
  }

  /**
   * 确保当前查询数据已就绪。
   * 首次挂载或跨路由切入时调用：若已有完整有效缓存则直接复用，否则触发刷新。
   */
  async function ensureCurrentQueryLoaded() {
    if (!store.hasInitialQueried) {
      await refreshCurrentQuery()
      return
    }

    // 仅当缓存非空且各维度数据（资产页、时间线、几何元数据）均满足当前视图时才免请求复用
    const hasAssets = store.totalCount > 0 && store.paginatedAssets.size > 0
    const hasTimeline =
      !store.isTimelineMode || (store.totalCount > 0 && store.timelineBuckets.length > 0)
    const requiresLayoutMeta = store.view.mode === 'masonry' || store.view.mode === 'adaptive'
    const hasLayoutMeta =
      !requiresLayoutMeta ||
      (store.totalCount > 0 && store.layoutMetaItems.length === store.totalCount)

    if (hasAssets && hasTimeline && hasLayoutMeta) {
      return
    }

    await refreshCurrentQuery()
  }

  function cancelNotificationRefresh() {
    scheduleNotificationRefresh.cancel()
  }

  /**
   * 加载指定页（带查询版本隔离与并发去重）
   */
  async function loadPage(pageNum: number): Promise<void> {
    if (store.isPageLoaded(pageNum)) {
      return
    }

    const requestVersion = store.queryVersion
    const requestKey = `${requestVersion}:${pageNum}`

    const existingPromise = inFlightPageLoads.get(requestKey)
    if (existingPromise) {
      return existingPromise
    }

    const loadPromise = (async () => {
      try {
        const response = await queryAssetPage(pageNum)
        if (!store.isQueryVersionCurrent(requestVersion)) {
          return
        }

        store.setPageAssets(pageNum, response.items)
        void refreshDyeCodeStatuses(response.items, requestVersion)

        console.log('✅ 第', pageNum, '页加载完成:', response.items.length, '个资产')
      } catch (error) {
        console.error('加载第', pageNum, '页失败:', error)
        throw error
      } finally {
        inFlightPageLoads.delete(requestKey)
      }
    })()

    inFlightPageLoads.set(requestKey, loadPromise)
    return loadPromise
  }

  /**
   * 确保指定的一组全局资产索引已加载完成。
   * 虚拟列表滚动时调用，内部自动换算缺失页码并并发拉取。
   */
  async function ensureIndexesLoaded(indexes: number[]): Promise<void> {
    if (indexes.length === 0 || store.perPage <= 0) return

    const neededPages = new Set(indexes.map((idx) => Math.floor(idx / store.perPage) + 1))
    await Promise.all([...neededPages].map((pageNum) => loadPage(pageNum)))
  }

  /**
   * 加载文件夹树
   */
  async function loadFolderTree() {
    try {
      store.setFoldersError(null)
      const folderTree = await galleryApi.getFolderTree()
      store.setFolders(folderTree)
    } catch (error) {
      console.error('Failed to load folder tree:', error)
      store.setFoldersError(t('gallery.sidebar.folders.loadFailed'))
      throw error
    }
  }

  /**
   * 加载标签树
   */
  async function loadTagTree() {
    try {
      store.setTagsError(null)
      const tagTree = await galleryApi.getTagTree()
      store.setTags(tagTree)
    } catch (error) {
      console.error('Failed to load tag tree:', error)
      store.setTagsError(t('gallery.sidebar.tags.loadFailed'))
      throw error
    }
  }

  /**
   * 扫描资产目录
   */
  async function scanAssets(options: ScanAssetsParams) {
    try {
      const result = await galleryApi.scanAssets(options)

      return result
    } catch (error) {
      console.error('Failed to scan assets:', error)
      throw error
    }
  }

  /**
   * 提交后台扫描任务
   */
  async function startScanAssets(options: ScanAssetsParams) {
    try {
      const result = await galleryApi.startScanAssets(options)

      return result
    } catch (error) {
      console.error('Failed to start scan task:', error)
      throw error
    }
  }

  /**
   * 获取资产缩略图URL
   */
  function getAssetThumbnailUrl(asset: any) {
    return galleryApi.getAssetThumbnailUrl(asset)
  }

  function getAssetUrl(asset: Asset) {
    return galleryApi.getAssetUrl(asset)
  }

  return {
    // 数据加载方法
    loadTimelineData,
    loadAllAssets,
    refreshCurrentQuery,
    ensureCurrentQueryLoaded,
    scheduleNotificationRefresh,
    cancelNotificationRefresh,
    loadPage,
    ensureIndexesLoaded,
    reloadLayoutMeta,
    ensureLayoutMetaLoaded,
    queryCurrentAssetIds,
    loadFolderTree,
    loadTagTree,
    scanAssets,
    startScanAssets,

    // 工具函数
    getAssetThumbnailUrl,
    getAssetUrl,
  }
}
