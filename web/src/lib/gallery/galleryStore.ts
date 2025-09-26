import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type {
  Asset,
  ViewConfig,
  AssetFilter,
  SelectionState,
  LightboxState,
  SidebarState,
  SortBy,
  SortOrder,
} from './types'

// Store状态接口
interface GalleryState {
  // 资产数据
  assets: Asset[]
  isLoading: boolean
  isInitialLoading: boolean
  error: string | null
  totalCount: number
  currentPage: number
  hasNextPage: boolean

  // 视图配置
  viewConfig: ViewConfig
  filter: AssetFilter
  sortBy: SortBy
  sortOrder: SortOrder

  // 选择状态
  selection: SelectionState

  // Lightbox状态
  lightbox: LightboxState

  // UI状态
  sidebar: SidebarState
  detailsOpen: boolean
}

interface GalleryActions {
  // 数据操作
  setAssets: (assets: Asset[]) => void
  addAssets: (assets: Asset[]) => void
  updateAsset: (id: number, updates: Partial<Asset>) => void
  removeAsset: (id: number) => void

  // 状态操作
  setLoading: (loading: boolean) => void
  setInitialLoading: (loading: boolean) => void
  setError: (error: string | null) => void
  setPagination: (totalCount: number, currentPage: number, hasNextPage: boolean) => void

  // 视图操作
  setViewConfig: (config: Partial<ViewConfig>) => void
  setFilter: (filter: Partial<AssetFilter>) => void
  setSorting: (sortBy: SortBy, sortOrder: SortOrder) => void

  // 选择操作
  selectAsset: (id: number, selected: boolean, multi?: boolean) => void
  selectAll: () => void
  clearSelection: () => void
  setActiveAsset: (id?: number) => void

  // Lightbox操作
  openLightbox: (assets: Asset[], startIndex: number) => void
  closeLightbox: () => void
  goToLightboxIndex: (index: number) => void
  goToPreviousLightbox: () => void
  goToNextLightbox: () => void

  // UI操作
  setSidebarOpen: (open: boolean) => void
  setSidebarActiveSection: (section: SidebarState['activeSection']) => void
  setDetailsOpen: (open: boolean) => void

  // 重置操作
  reset: () => void
}

type GalleryStoreType = GalleryState & GalleryActions

// 初始状态
const initialState: GalleryState = {
  assets: [],
  isLoading: false,
  isInitialLoading: false,
  error: null,
  totalCount: 0,
  currentPage: 1,
  hasNextPage: false,

  viewConfig: {
    mode: 'grid',
    size: 3,
  },
  filter: {},
  sortBy: 'created_at',
  sortOrder: 'desc',

  selection: {
    selectedIds: new Set<number>(),
    activeId: undefined,
    lastSelectedId: undefined,
  },

  lightbox: {
    isOpen: false,
    currentIndex: 0,
    assets: [],
  },

  sidebar: {
    isOpen: true,
    activeSection: 'all',
  },
  detailsOpen: true,
}

export const useGalleryStore = create<GalleryStoreType>()(
  devtools(
    (set, get) => ({
      // 初始状态
      ...initialState,

      // 数据操作
      setAssets: (assets: Asset[]) => {
        set({ assets })
      },

      addAssets: (newAssets: Asset[]) => {
        const { assets } = get()
        const existingIds = new Set(assets.map((a) => a.id))
        const uniqueAssets = newAssets.filter((a) => !existingIds.has(a.id))
        set({ assets: [...assets, ...uniqueAssets] })
      },

      updateAsset: (id: number, updates: Partial<Asset>) => {
        const { assets } = get()
        const updatedAssets = assets.map((asset) =>
          asset.id === id ? { ...asset, ...updates } : asset
        )
        set({ assets: updatedAssets })
      },

      removeAsset: (id: number) => {
        const { assets, selection, lightbox } = get()

        // 移除资产
        const filteredAssets = assets.filter((a) => a.id !== id)

        // 清理选择状态
        const newSelectedIds = new Set(selection.selectedIds)
        newSelectedIds.delete(id)

        const newSelection = {
          ...selection,
          selectedIds: newSelectedIds,
          activeId: selection.activeId === id ? undefined : selection.activeId,
          lastSelectedId: selection.lastSelectedId === id ? undefined : selection.lastSelectedId,
        }

        // 更新lightbox
        let newLightbox = lightbox
        if (lightbox.isOpen) {
          const lightboxAssets = lightbox.assets.filter((a) => a.id !== id)
          let currentIndex = lightbox.currentIndex

          if (currentIndex >= lightboxAssets.length) {
            currentIndex = Math.max(0, lightboxAssets.length - 1)
          }

          newLightbox = {
            ...lightbox,
            assets: lightboxAssets,
            currentIndex,
            isOpen: lightboxAssets.length > 0,
          }
        }

        set({
          assets: filteredAssets,
          selection: newSelection,
          lightbox: newLightbox,
        })
      },

      // 状态操作
      setLoading: (isLoading: boolean) => {
        set({ isLoading })
      },

      setInitialLoading: (isInitialLoading: boolean) => {
        set({ isInitialLoading })
      },

      setError: (error: string | null) => {
        set({ error })
      },

      setPagination: (totalCount: number, currentPage: number, hasNextPage: boolean) => {
        set({ totalCount, currentPage, hasNextPage })
      },

      // 视图操作
      setViewConfig: (config: Partial<ViewConfig>) => {
        const { viewConfig } = get()
        set({ viewConfig: { ...viewConfig, ...config } })
      },

      setFilter: (filter: Partial<AssetFilter>) => {
        const current = get().filter
        set({ filter: { ...current, ...filter } })
      },

      setSorting: (sortBy: SortBy, sortOrder: SortOrder) => {
        set({ sortBy, sortOrder })
      },

      // 选择操作
      selectAsset: (id: number, selected: boolean, multi = false) => {
        const { selection } = get()
        const newSelectedIds = new Set(selection.selectedIds)

        if (!multi) {
          newSelectedIds.clear()
        }

        if (selected) {
          newSelectedIds.add(id)
        } else {
          newSelectedIds.delete(id)
        }

        set({
          selection: {
            ...selection,
            selectedIds: newSelectedIds,
            lastSelectedId: selected ? id : selection.lastSelectedId,
          },
        })
      },

      selectAll: () => {
        const { assets, selection } = get()
        const allIds = new Set(assets.map((asset) => asset.id))
        set({
          selection: {
            ...selection,
            selectedIds: allIds,
          },
        })
      },

      clearSelection: () => {
        const { selection } = get()
        set({
          selection: {
            ...selection,
            selectedIds: new Set(),
            lastSelectedId: undefined,
          },
        })
      },

      setActiveAsset: (activeId?: number) => {
        const { selection } = get()
        set({
          selection: {
            ...selection,
            activeId,
          },
        })
      },

      // Lightbox操作
      openLightbox: (assets: Asset[], startIndex: number) => {
        const validIndex = Math.max(0, Math.min(startIndex, assets.length - 1))
        set({
          lightbox: {
            isOpen: true,
            assets,
            currentIndex: validIndex,
          },
        })
      },

      closeLightbox: () => {
        set({
          lightbox: {
            isOpen: false,
            currentIndex: 0,
            assets: [],
          },
        })
      },

      goToLightboxIndex: (index: number) => {
        const { lightbox } = get()
        if (lightbox.isOpen && lightbox.assets.length > 0) {
          const validIndex = Math.max(0, Math.min(index, lightbox.assets.length - 1))
          set({
            lightbox: {
              ...lightbox,
              currentIndex: validIndex,
            },
          })
        }
      },

      goToPreviousLightbox: () => {
        const { lightbox } = get()
        if (lightbox.isOpen && lightbox.currentIndex > 0) {
          set({
            lightbox: {
              ...lightbox,
              currentIndex: lightbox.currentIndex - 1,
            },
          })
        }
      },

      goToNextLightbox: () => {
        const { lightbox } = get()
        if (lightbox.isOpen && lightbox.currentIndex < lightbox.assets.length - 1) {
          set({
            lightbox: {
              ...lightbox,
              currentIndex: lightbox.currentIndex + 1,
            },
          })
        }
      },

      // UI操作
      setSidebarOpen: (isOpen: boolean) => {
        const { sidebar } = get()
        set({
          sidebar: {
            ...sidebar,
            isOpen,
          },
        })
      },

      setSidebarActiveSection: (activeSection: SidebarState['activeSection']) => {
        const { sidebar } = get()
        set({
          sidebar: {
            ...sidebar,
            activeSection,
          },
        })
      },

      setDetailsOpen: (detailsOpen: boolean) => {
        set({ detailsOpen })
      },

      // 重置操作
      reset: () => {
        set({
          ...initialState,
          selection: {
            ...initialState.selection,
            selectedIds: new Set(),
          },
        })
      },
    }),
    {
      name: 'gallery-store',
      enabled: import.meta.env.DEV,
    }
  )
)

// 便捷选择器
export const useAssets = () => useGalleryStore((state) => state.assets)
export const useAssetsLoading = () => useGalleryStore((state) => state.isLoading)
export const useAssetsSelection = () => useGalleryStore((state) => state.selection)
export const useAssetsLightbox = () => useGalleryStore((state) => state.lightbox)
export const useAssetsViewConfig = () => useGalleryStore((state) => state.viewConfig)
