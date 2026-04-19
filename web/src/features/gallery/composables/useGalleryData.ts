import { useGalleryStore } from '../store'
import { galleryApi } from '../api'
import type { Asset, ScanAssetsParams } from '../types'
import { toQueryAssetsFilters } from '../queryFilters'

/**
 * Gallery数据管理 Composable
 * 负责协调 API 调用和 Store 操作
 * 组件应直接从 Store 读取状态，而不是通过这里的 computed 属性
 */
export function useGalleryData() {
  const store = useGalleryStore()

  function findLoadedAssetById(assetId: number) {
    for (const pageAssets of store.paginatedAssets.values()) {
      const asset = pageAssets.find((item) => item.id === assetId)
      if (asset) {
        return asset
      }
    }

    return undefined
  }

  function hasRenderableResults(): boolean {
    return (
      store.paginatedAssets.size > 0 || store.totalCount > 0 || store.timelineBuckets.length > 0
    )
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

  async function queryVisiblePages(total: number, preferredPage: number) {
    if (total <= 0) {
      return new Map<number, Asset[]>()
    }

    const maxPage = Math.max(1, Math.ceil(total / store.perPage))
    const visiblePages = new Set(getVisiblePageNumbers(total))
    visiblePages.add(Math.max(1, Math.min(preferredPage, maxPage)))
    const pageNumbers = [...visiblePages].sort((left, right) => left - right)

    const responses = await Promise.all(pageNumbers.map((pageNum) => queryAssetPage(pageNum)))
    const pages = new Map<number, Asset[]>()

    pageNumbers.forEach((pageNum, index) => {
      pages.set(pageNum, responses[index]?.items ?? [])
    })

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

    // 原资产已不在新结果集里：清空 active，并在灯箱场景下直接退出，避免跳到错误图片。
    if (activeAssetIndex === undefined) {
      if (store.lightbox.isOpen) {
        store.closeLightbox()
      }

      if (store.detailsPanel.type === 'asset' && store.detailsPanel.asset.id === activeAssetId) {
        store.clearDetailsFocus()
      }

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
      store.detailsPanel.asset.id === loadedActiveAsset.id
    ) {
      store.setDetailsFocus({ type: 'asset', asset: loadedActiveAsset })
    }

    if (store.lightbox.isOpen && loadedActiveAsset) {
      store.replaceSelection([loadedActiveAsset.id])
      store.setSelectionAnchor(activeAssetIndex)
      store.setDetailsFocus({ type: 'asset', asset: loadedActiveAsset })
    }
  }

  /**
   * 结果集有数据但右侧仍为空白（未选文件夹/标签）时，默认选中第一项，与常见资源管理器行为一致。
   * 不覆盖 folder/tag/batch 详情焦点。
   */
  function tryFocusFirstResultWhenDetailsEmpty(requestVersion: number) {
    if (!store.isQueryVersionCurrent(requestVersion)) {
      return
    }
    if (store.totalCount <= 0) {
      return
    }
    if (store.detailsPanel.type !== 'none') {
      return
    }
    if (store.selection.activeAssetId !== undefined) {
      return
    }
    if (store.selectedCount > 0) {
      return
    }

    const firstAsset = store.paginatedAssets.get(1)?.[0]
    if (!firstAsset) {
      return
    }

    store.replaceSelection([firstAsset.id])
    store.setSelectionAnchor(0)
    store.setActiveAsset(firstAsset.id, 0)
    store.setDetailsFocus({ type: 'asset', asset: firstAsset })
  }

  /**
   * 非时间线模式：按当前筛选/排序拉取 `queryAssets` 分页结果并写入 store。
   * 网格、列表、瀑布流、自适应等视图共用，与布局无关。
   */
  async function refreshPagedAssetQuery() {
    const requestVersion = store.beginQueryRefresh()
    const shouldShowLoading = !hasRenderableResults()

    try {
      if (shouldShowLoading) {
        store.setLoading(true)
      }
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

      const pages = await queryVisiblePages(response.totalCount, pageNum)
      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      {
        store.clearTimelineData()
        store.setPagination(response.totalCount, pageNum, pageNum < maxPage)
        store.replacePaginatedAssets(pages)
      }

      await reconcileActiveAsset(response.activeAssetIndex, requestVersion)
      tryFocusFirstResultWhenDetailsEmpty(requestVersion)

      console.log('📊 加载完成:', {
        totalCount: response.totalCount,
        loadedPages: [...pages.keys()],
        perPage: store.perPage,
      })
    } catch (error) {
      console.error('加载失败:', error)
      store.setError('加载数据失败')
    } finally {
      store.finishQueryRefresh(requestVersion)
      if (shouldShowLoading) {
        store.setLoading(false)
      }
    }
  }

  async function refreshTimelineData() {
    const requestVersion = store.beginQueryRefresh()
    const shouldShowLoading = !hasRenderableResults()

    try {
      if (shouldShowLoading) {
        store.setLoading(true)
      }
      store.setError(null)

      const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)
      const bucketsResponse = await galleryApi.getTimelineBuckets({
        folderId: filters.folderId,
        includeSubfolders: filters.includeSubfolders,
        sortOrder: store.sortOrder,
        activeAssetId: store.selection.activeAssetId,
        type: filters.type,
        search: filters.search,
        rating: filters.rating,
        reviewFlag: filters.reviewFlag,
        tagIds: filters.tagIds,
        tagMatchMode: filters.tagMatchMode,
        clothIds: filters.clothIds,
        clothMatchMode: filters.clothMatchMode,
        colorHexes: filters.colorHexes,
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
      const pages = await queryVisiblePages(bucketsResponse.totalCount, pageNum)
      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      {
        store.setTimelineBuckets(bucketsResponse.buckets)
        store.setTimelineTotalCount(bucketsResponse.totalCount)
        store.setPagination(
          bucketsResponse.totalCount,
          pageNum,
          pageNum < Math.max(1, Math.ceil(bucketsResponse.totalCount / store.perPage))
        )
        store.replacePaginatedAssets(pages)
      }

      await reconcileActiveAsset(bucketsResponse.activeAssetIndex, requestVersion)
      tryFocusFirstResultWhenDetailsEmpty(requestVersion)

      console.log('📅 时间线数据加载成功:', {
        months: bucketsResponse.buckets.length,
        total: bucketsResponse.totalCount,
        loadedPages: [...pages.keys()],
      })
    } catch (error) {
      console.error('Failed to load timeline data:', error)
      store.setError('加载时间线数据失败')
    } finally {
      store.finishQueryRefresh(requestVersion)
      if (shouldShowLoading) {
        store.setLoading(false)
      }
    }
  }

  // ============= 数据加载操作 =============

  /**
   * 加载时间线数据（月份元数据 + 当前可见页）
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
   * 加载指定页（用于虚拟列表按需加载）
   */
  async function loadPage(pageNum: number) {
    if (store.isPageLoaded(pageNum)) {
      return
    }

    const requestVersion = store.queryVersion

    try {
      const response = await queryAssetPage(pageNum)
      if (!store.isQueryVersionCurrent(requestVersion)) {
        return
      }

      store.setPageAssets(pageNum, response.items)

      console.log('✅ 第', pageNum, '页加载完成:', response.items.length, '个资产')
    } catch (error) {
      console.error('加载第', pageNum, '页失败:', error)
      throw error
    }
  }

  /**
   * 加载文件夹树
   */
  async function loadFolderTree(options: { silent?: boolean } = {}) {
    const { silent = false } = options

    try {
      if (!silent) {
        store.setFoldersLoading(true)
        store.setFoldersError(null)
      }

      const folderTree = await galleryApi.getFolderTree()
      store.setFolders(folderTree)
    } catch (error) {
      console.error('Failed to load folder tree:', error)
      if (!silent) {
        store.setFoldersError('加载文件夹树失败')
      }
      throw error
    } finally {
      if (!silent) {
        store.setFoldersLoading(false)
      }
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
    loadPage,
    queryCurrentAssetIds,
    loadFolderTree,
    scanAssets,
    startScanAssets,

    // 工具函数
    getAssetThumbnailUrl,
    getAssetUrl,
  }
}
