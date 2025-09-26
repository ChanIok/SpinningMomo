import { useCallback, useEffect } from 'react'
import { useGalleryStore } from '../galleryStore'
import { listAssets, getAssetStats } from '../galleryApi'
import { createMockListResponse, mockAssetStats } from '../mockData'
import type { ListAssetsParams, AssetStats } from '../types'

interface UseAssetsOptions {
  useMockData?: boolean
  autoLoad?: boolean
  initialParams?: ListAssetsParams
}

export function useAssets(options: UseAssetsOptions = {}) {
  const {
    useMockData = true, // 默认使用mock数据进行开发
    autoLoad = true,
    initialParams = {},
  } = options

  // 从store获取状态和操作
  const store = useGalleryStore()

  // 加载资产列表
  const loadAssets = useCallback(
    async (params: ListAssetsParams = {}) => {
      try {
        store.setLoading(true)
        store.setError(null)

        if (useMockData) {
          // 使用mock数据
          await new Promise((resolve) => setTimeout(resolve, 500)) // 模拟网络延迟

          const mockResponse = createMockListResponse(params.page, params.per_page)
          store.setAssets(mockResponse.items)
          store.setPagination(
            mockResponse.total_count,
            mockResponse.current_page,
            mockResponse.current_page < mockResponse.total_pages
          )
        } else {
          // 调用真实API
          const response = await listAssets({
            page: params.page || 1,
            per_page: params.per_page || 20,
            sort_by: store.sortBy,
            sort_order: store.sortOrder,
            ...store.filter,
            ...params,
          })

          store.setAssets(response.items)
          store.setPagination(
            response.total_count,
            response.current_page,
            response.current_page < response.total_pages
          )
        }
      } catch (error) {
        const errorMessage = error instanceof Error ? error.message : '加载资产失败'
        store.setError(errorMessage)
        console.error('Failed to load assets:', error)
      } finally {
        store.setLoading(false)
      }
    },
    [useMockData, store]
  )

  // 加载更多资产
  const loadMore = useCallback(async () => {
    if (!store.hasNextPage || store.isLoading) return

    try {
      store.setLoading(true)

      const nextPage = store.currentPage + 1

      if (useMockData) {
        await new Promise((resolve) => setTimeout(resolve, 300))

        const mockResponse = createMockListResponse(nextPage, 20)
        store.addAssets(mockResponse.items)
        store.setPagination(mockResponse.total_count, nextPage, nextPage < mockResponse.total_pages)
      } else {
        const response = await listAssets({
          page: nextPage,
          per_page: 20,
          sort_by: store.sortBy,
          sort_order: store.sortOrder,
          ...store.filter,
        })

        store.addAssets(response.items)
        store.setPagination(response.total_count, nextPage, nextPage < response.total_pages)
      }
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : '加载更多资产失败'
      store.setError(errorMessage)
      console.error('Failed to load more assets:', error)
    } finally {
      store.setLoading(false)
    }
  }, [useMockData, store])

  // 刷新资产列表
  const refreshAssets = useCallback(() => {
    return loadAssets({ ...initialParams, page: 1 })
  }, [loadAssets, initialParams])

  // 获取统计信息
  const loadStats = useCallback(async (): Promise<AssetStats | null> => {
    try {
      if (useMockData) {
        await new Promise((resolve) => setTimeout(resolve, 200))
        return mockAssetStats
      } else {
        return await getAssetStats()
      }
    } catch (error) {
      console.error('Failed to load asset stats:', error)
      return null
    }
  }, [useMockData])

  // 应用筛选器
  const applyFilter = useCallback(
    (newFilter: Partial<typeof store.filter>) => {
      store.setFilter(newFilter)
      // 筛选后重新加载第一页
      loadAssets({ page: 1 })
    },
    [store, loadAssets]
  )

  // 应用排序
  const applySorting = useCallback(
    (sortBy: typeof store.sortBy, sortOrder: typeof store.sortOrder) => {
      store.setSorting(sortBy, sortOrder)
      // 排序后重新加载第一页
      loadAssets({ page: 1 })
    },
    [store, loadAssets]
  )

  // 初始加载
  useEffect(() => {
    if (autoLoad && store.assets.length === 0 && !store.isInitialLoading) {
      store.setInitialLoading(true)
      loadAssets(initialParams).finally(() => {
        store.setInitialLoading(false)
      })
    }
  }, [autoLoad, initialParams, loadAssets, store])

  return {
    // 状态
    assets: store.assets,
    isLoading: store.isLoading,
    isInitialLoading: store.isInitialLoading,
    error: store.error,
    totalCount: store.totalCount,
    currentPage: store.currentPage,
    hasNextPage: store.hasNextPage,
    filter: store.filter,
    sortBy: store.sortBy,
    sortOrder: store.sortOrder,

    // 操作
    loadAssets,
    loadMore,
    refreshAssets,
    loadStats,
    applyFilter,
    applySorting,
  }
}
