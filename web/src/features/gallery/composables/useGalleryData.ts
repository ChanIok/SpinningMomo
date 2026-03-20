import { useGalleryStore } from '../store'
import { galleryApi } from '../api'
import type { ScanAssetsParams } from '../types'
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

  // 在筛选结果刷新后，用 activeAssetId 将当前位置重建到新的结果集上。
  async function reconcileActiveAsset(activeAssetIndex?: number) {
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

  // ============= 数据加载操作 =============

  /**
   * 加载时间线数据（月份元数据 + 第一页）
   */
  async function loadTimelineData() {
    try {
      store.setLoading(true)
      store.setError(null)

      // 清空分页缓存（重新加载时）
      store.clearPaginatedAssets()

      const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

      // 1. 获取月份元数据（使用完整过滤条件）
      const response = await galleryApi.getTimelineBuckets({
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

      store.setTimelineBuckets(response.buckets)
      store.setTimelineTotalCount(response.totalCount)

      // 2. 设置分页总数（用于虚拟滚动）
      store.setPagination(response.totalCount, 1, false)

      await reconcileActiveAsset(response.activeAssetIndex)

      console.log('📅 时间线数据加载成功:', {
        months: response.buckets.length,
        total: response.totalCount,
      })
    } catch (error) {
      console.error('Failed to load timeline data:', error)
      store.setError('加载时间线数据失败')
    } finally {
      store.setLoading(false)
    }
  }

  /**
   * 加载普通模式资产 - 首次请求获取总数和第一页
   */
  async function loadAllAssets() {
    try {
      store.setLoading(true)
      store.setError(null)

      // 清空时间线数据
      store.clearTimelineData()

      // 清空分页缓存（重新加载时）
      store.clearPaginatedAssets()

      const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

      // 首次请求获取总数和第一页
      const response = await galleryApi.queryAssets({
        filters,
        sortBy: store.sortBy,
        sortOrder: store.sortOrder,
        activeAssetId: store.selection.activeAssetId,
        page: 1,
        perPage: store.perPage,
      })

      // 设置总数（用于构建虚拟列表）
      store.setPagination(response.totalCount, 1, false)

      // 缓存第一页数据
      store.setPageAssets(1, response.items)

      await reconcileActiveAsset(response.activeAssetIndex)

      console.log('📊 加载完成:', {
        totalCount: response.totalCount,
        firstPage: response.items.length,
        perPage: store.perPage,
      })
    } catch (error) {
      console.error('加载失败:', error)
      store.setError('加载数据失败')
    } finally {
      store.setLoading(false)
    }
  }

  /**
   * 加载指定页（用于虚拟列表按需加载）
   */
  async function loadPage(pageNum: number) {
    if (store.isPageLoaded(pageNum)) {
      return
    }

    try {
      const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

      const response = await galleryApi.queryAssets({
        filters,
        sortBy: store.sortBy,
        sortOrder: store.sortOrder,
        page: pageNum,
        perPage: store.perPage,
      })

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
  async function loadFolderTree() {
    try {
      store.setFoldersLoading(true)
      store.setFoldersError(null)

      const folderTree = await galleryApi.getFolderTree()
      store.setFolders(folderTree)
    } catch (error) {
      console.error('Failed to load folder tree:', error)
      store.setFoldersError('加载文件夹树失败')
      throw error
    } finally {
      store.setFoldersLoading(false)
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

  function getAssetUrl(assetId: number) {
    return galleryApi.getAssetUrl(assetId)
  }

  return {
    // 数据加载方法
    loadTimelineData, // 时间线元数据
    loadAllAssets, // 普通模式首次加载
    loadPage, // 加载指定页（虚拟列表用）
    loadFolderTree, // 文件夹树
    scanAssets,
    startScanAssets,

    // 工具函数
    getAssetThumbnailUrl,
    getAssetUrl,
  }
}
