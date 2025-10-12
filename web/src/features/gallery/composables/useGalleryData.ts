import { computed } from 'vue'
import { useGalleryStore } from '../store'
import { galleryApi } from '../api'
import type { ListAssetsParams } from '../types'

/**
 * Gallery数据管理 Composable
 * 负责从后端获取和管理资产数据
 */
export function useGalleryData() {
  const store = useGalleryStore()

  // ============= 状态 =============
  const assets = computed(() => store.assets)
  const isLoading = computed(() => store.isLoading)
  const isInitialLoading = computed(() => store.isInitialLoading)
  const error = computed(() => store.error)
  const totalCount = computed(() => store.totalCount)
  const currentPage = computed(() => store.currentPage)
  const hasNextPage = computed(() => store.hasNextPage)

  // 时间线状态
  const isTimelineMode = computed(() => store.isTimelineMode)
  const timelineBuckets = computed(() => store.timelineBuckets)
  const timelineTotalCount = computed(() => store.timelineTotalCount)

  // 文件夹树状态
  const folders = computed(() => store.folders)
  const foldersLoading = computed(() => store.foldersLoading)
  const foldersError = computed(() => store.foldersError)

  // ============= 数据加载操作 =============

  /**
   * 统一加载入口 - 根据模式自动选择加载方式
   */
  async function load() {
    if (isTimelineMode.value) {
      return loadTimelineData()
    } else {
      return loadAssets({ page: 1, perPage: 50 })
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
   * 加载资产列表（普通分页模式）
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

  return {
    // 状态
    assets,
    isLoading,
    isInitialLoading,
    error,
    totalCount,
    currentPage,
    hasNextPage,

    // 时间线状态
    isTimelineMode,
    timelineBuckets,
    timelineTotalCount,

    // 文件夹树状态
    folders,
    foldersLoading,
    foldersError,

    // 操作
    load, // 统一加载入口
    loadTimelineData, // 时间线元数据
    loadMonthAssets, // 月份数据
    loadAssets, // 普通分页
    loadFolderTree, // 文件夹树
    initialize,
    reload,
    loadMore,
    scanAssets,
  }
}
