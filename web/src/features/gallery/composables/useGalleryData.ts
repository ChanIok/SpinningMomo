import { useGalleryStore } from '../store'
import { galleryApi } from '../api'
import type { ListAssetsParams } from '../types'

/**
 * Gallery数据管理 Composable
 * 负责协调 API 调用和 Store 操作
 * 组件应直接从 Store 读取状态，而不是通过这里的 computed 属性
 */
export function useGalleryData() {
  const store = useGalleryStore()

  // ============= 数据加载操作 =============

  /**
   * 统一加载入口 - 根据模式自动选择加载方式
   */
  async function load() {
    if (store.isTimelineMode) {
      return loadTimelineData()
    } else {
      return loadAllAssets() // 使用新的分页加载方式
    }
  }

  /**
   * 加载时间线数据（月份元数据）
   */
  async function loadTimelineData() {
    try {
      store.setLoading(true)
      store.setError(null)

      // 清空普通模式的数据（节省内存）
      store.setAssets([])

      const response = await galleryApi.getTimelineBuckets({
        folderId: store.filter.folderId ? Number(store.filter.folderId) : undefined,
        includeSubfolders: store.includeSubfolders,
      })

      store.setTimelineBuckets(response.buckets)
      store.setTimelineTotalCount(response.totalCount)

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
   * 加载指定月份的资产数据
   */
  async function loadMonthAssets(month: string) {
    const folderId = store.filter.folderId ? Number(store.filter.folderId) : undefined
    const includeSubfolders = store.includeSubfolders

    // 检查是否已加载（使用复合缓存键）
    const cachedAssets = store.getMonthAssets(month, folderId, includeSubfolders)
    if (cachedAssets?.length) {
      const cacheKey = store.getMonthCacheKey(month, folderId, includeSubfolders)
      console.log('⏭️ 月份数据已缓存:', cacheKey)
      return
    }

    try {
      const cacheKey = store.getMonthCacheKey(month, folderId, includeSubfolders)
      console.log('📸 加载月份数据:', cacheKey)

      const response = await galleryApi.getAssetsByMonth({
        month: month,
        folderId: folderId,
        includeSubfolders: includeSubfolders,
        sortOrder: 'desc',
      })

      store.setMonthAssets(month, response.assets, folderId, includeSubfolders)

      console.log('✅ 月份数据加载完成:', cacheKey, response.count)
    } catch (error) {
      console.error('加载月份数据失败:', month, error)
      throw error
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

      // 首次请求获取总数和第一页
      const response = await galleryApi.queryAssets({
        filters: {
          folderId: store.filter.folderId ? Number(store.filter.folderId) : undefined,
          includeSubfolders: store.includeSubfolders,
          type: store.filter.type,
          search: store.filter.searchQuery,
        },
        sortBy: store.sortBy,
        sortOrder: store.sortOrder,
        page: 1,
        perPage: store.perPage,
      })

      // 设置总数（用于构建虚拟列表）
      store.setPagination(response.totalCount, 1, false)

      // 缓存第一页数据
      store.setPageAssets(1, response.items)

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

    console.log('📄 加载第', pageNum, '页')

    try {
      const response = await galleryApi.queryAssets({
        filters: {
          folderId: store.filter.folderId ? Number(store.filter.folderId) : undefined,
          includeSubfolders: store.includeSubfolders,
          type: store.filter.type,
          search: store.filter.searchQuery,
        },
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
   * 加载资产列表（普通分页模式 - 保留用于兼容）
   */
  async function loadAssets(params: ListAssetsParams = {}) {
    try {
      store.setLoading(true)
      store.setError(null)

      // 清空时间线数据（节省内存）
      store.clearTimelineData()

      const response = await galleryApi.listAssets(params)

      // 第一页替换，其他页追加
      if (params.page === 1) {
        store.setAssets(response.items)
      } else {
        store.addAssets(response.items)
      }

      store.setPagination(
        response.totalCount,
        response.currentPage,
        response.currentPage < response.totalPages
      )

      console.log('🖼️ Gallery数据加载成功:', {
        count: response.items.length,
        total: response.totalCount,
        page: response.currentPage,
      })
    } catch (error) {
      console.error('Failed to load gallery assets:', error)
      store.setError('加载资产数据失败')
    } finally {
      store.setLoading(false)
    }
  }

  /**
   * 初始化加载
   */
  async function initialize() {
    try {
      store.setInitialLoading(true)
      await load()
    } catch (error) {
      console.error('Failed to initialize gallery:', error)
    } finally {
      store.setInitialLoading(false)
    }
  }

  /**
   * 重新加载（保持当前模式和筛选）
   */
  async function reload() {
    return load()
  }

  /**
   * 加载更多（分页）
   */
  async function loadMore() {
    if (!store.hasNextPage || store.isLoading) return

    const nextParams: ListAssetsParams = {
      page: store.currentPage + 1,
      perPage: 50,
      sortBy: store.sortBy,
      sortOrder: store.sortOrder,
      folderId: store.filter.folderId ? Number(store.filter.folderId) : undefined,
      includeSubfolders: store.includeSubfolders,
    }

    return loadAssets(nextParams)
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

      console.log('📁 文件夹树加载成功:', folderTree.length)
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
  async function scanAssets(directories: string[]) {
    try {
      const result = await galleryApi.scanAssets({
        directories,
        recursive: true,
        generateThumbnails: true,
      })

      // 扫描完成后重新加载列表
      await reload()

      return result
    } catch (error) {
      console.error('Failed to scan assets:', error)
      throw error
    }
  }

  /**
   * 获取资产缩略图URL
   */
  function getAssetThumbnailUrl(asset: any) {
    return galleryApi.getAssetThumbnailUrl(asset)
  }

  return {
    // 数据加载方法
    load, // 统一加载入口
    loadTimelineData, // 时间线元数据
    loadMonthAssets, // 月份数据
    loadAllAssets, // 普通模式首次加载
    loadPage, // 加载指定页（虚拟列表用）
    loadAssets, // 普通分页（保留兼容）
    loadFolderTree, // 文件夹树
    initialize,
    reload,
    loadMore,
    scanAssets,

    // 工具函数
    getAssetThumbnailUrl,

    // 虚拟滚动所需方法（直接传递 store 方法）
    getAssetsInRange: (start: number, end: number) => store.getAssetsInRange(start, end),
    isPageLoaded: (pageNum: number) => store.isPageLoaded(pageNum),
    getPerPage: () => store.perPage,
  }
}
