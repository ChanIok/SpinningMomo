import { useEffect, useCallback } from 'react'
import { useGalleryStore } from '../galleryStore'
import { listAssets, getAssetStats } from '../galleryApi'
import type { ListAssetsParams } from '../types'

/**
 * Gallery数据管理hook
 * 负责从后端获取真实的资产数据，替换mock数据
 */
export function useGalleryData() {
  const store = useGalleryStore()

  // 加载资产列表
  const loadAssets = useCallback(
    async (params: ListAssetsParams = {}) => {
      try {
        store.setLoading(true)
        store.setError(null)

        const response = await listAssets(params)

        // 直接使用后端数据
        const assets = response.items

        if (params.page === 1) {
          store.setAssets(assets)
        } else {
          store.addAssets(assets)
        }

        store.setPagination(
          response.totalCount,
          response.currentPage,
          response.currentPage < response.totalPages
        )

        console.log('🖼️ Gallery数据加载成功:', {
          count: assets.length,
          total: response.totalCount,
          page: response.currentPage,
        })
      } catch (error) {
        console.error('Failed to load gallery assets:', error)
        store.setError('加载资产数据失败')
      } finally {
        store.setLoading(false)
      }
    },
    [store]
  )

  // 初始加载
  const initialize = useCallback(async () => {
    try {
      store.setInitialLoading(true)
      await loadAssets({ page: 1, perPage: 50 })
    } catch (error) {
      console.error('Failed to initialize gallery:', error)
    } finally {
      store.setInitialLoading(false)
    }
  }, [loadAssets, store])

  // 重新加载（保持当前筛选和排序）
  const reload = useCallback(() => {
    const currentParams: ListAssetsParams = {
      page: 1,
      perPage: 50,
      sortBy: store.sortBy,
      sortOrder: store.sortOrder,
      filterType: store.filter.type,
      searchQuery: store.filter.searchQuery,
    }
    return loadAssets(currentParams)
  }, [loadAssets, store.sortBy, store.sortOrder, store.filter])

  // 加载更多（分页）
  const loadMore = useCallback(() => {
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
  }, [loadAssets, store])

  // 加载统计数据
  const loadStats = useCallback(async () => {
    try {
      const stats = await getAssetStats()
      console.log('📊 Gallery统计数据:', stats)
      return stats
    } catch (error) {
      console.error('Failed to load gallery stats:', error)
      throw error
    }
  }, [])

  return {
    // 状态
    assets: store.assets,
    isLoading: store.isLoading,
    isInitialLoading: store.isInitialLoading,
    error: store.error,
    totalCount: store.totalCount,
    currentPage: store.currentPage,
    hasNextPage: store.hasNextPage,

    // 操作
    initialize,
    reload,
    loadMore,
    loadStats,
  }
}

/**
 * 自动初始化的Gallery数据hook
 * 组件挂载时自动加载数据
 */
export function useAutoGalleryData() {
  const galleryData = useGalleryData()

  // 自动初始化
  useEffect(() => {
    if (galleryData.assets.length === 0 && !galleryData.isInitialLoading) {
      galleryData.initialize()
    }
  }, [galleryData])

  return galleryData
}
