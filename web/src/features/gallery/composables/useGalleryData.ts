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

  // ============= 数据加载操作 =============
  
  /**
   * 加载资产列表
   */
  async function loadAssets(params: ListAssetsParams = {}) {
    try {
      store.setLoading(true)
      store.setError(null)

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
      await loadAssets({ page: 1, perPage: 50 })
    } catch (error) {
      console.error('Failed to initialize gallery:', error)
    } finally {
      store.setInitialLoading(false)
    }
  }

  /**
   * 重新加载（保持当前筛选和排序）
   */
  async function reload() {
    const currentParams: ListAssetsParams = {
      page: 1,
      perPage: 50,
      sortBy: store.sortBy,
      sortOrder: store.sortOrder,
      filterType: store.filter.type,
      searchQuery: store.filter.searchQuery,
    }
    return loadAssets(currentParams)
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
      filterType: store.filter.type,
      searchQuery: store.filter.searchQuery,
    }

    return loadAssets(nextParams)
  }

  /**
   * 删除资产
   */
  async function deleteAsset(id: number, deleteFile = false) {
    try {
      store.setLoading(true)
      
      await galleryApi.deleteAsset({ id, deleteFile })
      
      // 从 store 中移除资产
      store.removeAsset(id)
      
      console.log('✅ 资产删除成功:', id)
    } catch (error) {
      console.error('Failed to delete asset:', error)
      store.setError('删除资产失败')
      throw error
    } finally {
      store.setLoading(false)
    }
  }

  /**
   * 批量删除选中的资产
   */
  async function deleteSelectedAssets(deleteFile = false) {
    const selectedIds = Array.from(store.selection.selectedIds)
    if (selectedIds.length === 0) return

    try {
      store.setLoading(true)
      
      // 并发删除
      const promises = selectedIds.map(id => 
        galleryApi.deleteAsset({ id, deleteFile })
      )
      
      await Promise.all(promises)
      
      // 从 store 中移除资产
      selectedIds.forEach(id => store.removeAsset(id))
      
      // 清空选择
      store.clearSelection()
      
      console.log('✅ 批量删除成功:', selectedIds.length)
    } catch (error) {
      console.error('Failed to delete selected assets:', error)
      store.setError('批量删除失败')
      throw error
    } finally {
      store.setLoading(false)
    }
  }

  /**
   * 加载统计数据
   */
  async function loadStats() {
    try {
      const stats = await galleryApi.getAssetStats()
      console.log('📊 Gallery统计数据:', stats)
      return stats
    } catch (error) {
      console.error('Failed to load gallery stats:', error)
      throw error
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
        generateThumbnails: true 
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

    // 操作
    initialize,
    reload,
    loadMore,
    deleteAsset,
    deleteSelectedAssets,
    loadStats,
    scanAssets,
  }
}
