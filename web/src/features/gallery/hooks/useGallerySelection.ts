import { useCallback, useMemo } from 'react'
import { useGalleryStore } from '@/lib/gallery/galleryStore'
import type { Asset } from '@/lib/gallery/types'

export function useGallerySelection() {
  // 从 store 获取状态和操作
  const assets = useGalleryStore((state) => state.assets)
  const selection = useGalleryStore((state) => state.selection)
  const selectAsset = useGalleryStore((state) => state.selectAsset)
  const selectAll = useGalleryStore((state) => state.selectAll)
  const clearSelection = useGalleryStore((state) => state.clearSelection)
  const setActiveAsset = useGalleryStore((state) => state.setActiveAsset)

  // 计算选择状态
  const selectedAssets = useMemo(() => {
    return assets.filter((asset) => selection.selectedIds.has(asset.id))
  }, [assets, selection.selectedIds])

  const selectedCount = selection.selectedIds.size
  const isAllSelected = selectedCount > 0 && selectedCount === assets.length
  const isPartiallySelected = selectedCount > 0 && selectedCount < assets.length

  // 基础选择操作
  const handleAssetSelect = useCallback(
    (assetId: number, selected: boolean, multi: boolean = false) => {
      selectAsset(assetId, selected, multi)
    },
    [selectAsset]
  )

  // 单击资产 - 设置活跃状态
  const handleAssetClick = useCallback(
    (asset: Asset) => {
      setActiveAsset(asset.id)
    },
    [setActiveAsset]
  )

  // 切换单个资产选择状态
  const toggleAssetSelection = useCallback(
    (assetId: number) => {
      const isSelected = selection.selectedIds.has(assetId)
      handleAssetSelect(assetId, !isSelected, true)
    },
    [selection.selectedIds, handleAssetSelect]
  )

  // 切换全选
  const toggleSelectAll = useCallback(() => {
    if (isAllSelected) {
      clearSelection()
    } else {
      selectAll()
    }
  }, [isAllSelected, clearSelection, selectAll])

  // 范围选择 - 从最后选择的到目标资产
  const selectRange = useCallback(
    (targetId: number) => {
      const { lastSelectedId } = selection
      if (!lastSelectedId) {
        handleAssetSelect(targetId, true, false)
        return
      }

      const lastIndex = assets.findIndex((asset) => asset.id === lastSelectedId)
      const targetIndex = assets.findIndex((asset) => asset.id === targetId)

      if (lastIndex === -1 || targetIndex === -1) return

      const startIndex = Math.min(lastIndex, targetIndex)
      const endIndex = Math.max(lastIndex, targetIndex)

      // 清除当前选择
      clearSelection()

      // 选择范围内的所有资产
      for (let i = startIndex; i <= endIndex; i++) {
        selectAsset(assets[i].id, true, true)
      }
    },
    [assets, selection.lastSelectedId, handleAssetSelect, clearSelection, selectAsset]
  )

  // 键盘导航选择
  const navigateSelection = useCallback(
    (direction: 'up' | 'down' | 'left' | 'right', columnsPerRow?: number) => {
      const { activeId } = selection
      if (!activeId || assets.length === 0) return

      const currentIndex = assets.findIndex((asset) => asset.id === activeId)
      if (currentIndex === -1) return

      let nextIndex = currentIndex
      const totalAssets = assets.length

      switch (direction) {
        case 'left':
          nextIndex = Math.max(0, currentIndex - 1)
          break
        case 'right':
          nextIndex = Math.min(totalAssets - 1, currentIndex + 1)
          break
        case 'up':
          if (columnsPerRow) {
            nextIndex = Math.max(0, currentIndex - columnsPerRow)
          } else {
            nextIndex = Math.max(0, currentIndex - 1)
          }
          break
        case 'down':
          if (columnsPerRow) {
            nextIndex = Math.min(totalAssets - 1, currentIndex + columnsPerRow)
          } else {
            nextIndex = Math.min(totalAssets - 1, currentIndex + 1)
          }
          break
      }

      if (nextIndex !== currentIndex) {
        setActiveAsset(assets[nextIndex].id)
      }
    },
    [assets, selection.activeId, setActiveAsset]
  )

  // 选择当前活跃的资产
  const selectActiveAsset = useCallback(() => {
    const { activeId } = selection
    if (activeId) {
      toggleAssetSelection(activeId)
    }
  }, [selection.activeId, toggleAssetSelection])

  // 根据条件选择资产
  const selectByCondition = useCallback(
    (predicate: (asset: Asset) => boolean) => {
      clearSelection()
      assets.forEach((asset) => {
        if (predicate(asset)) {
          selectAsset(asset.id, true, true)
        }
      })
    },
    [assets, clearSelection, selectAsset]
  )

  // 按类型选择
  const selectByType = useCallback(
    (type: Asset['type']) => {
      selectByCondition((asset) => asset.type === type)
    },
    [selectByCondition]
  )

  // 反选
  const invertSelection = useCallback(() => {
    const currentSelected = new Set(selection.selectedIds)
    clearSelection()
    assets.forEach((asset) => {
      if (!currentSelected.has(asset.id)) {
        selectAsset(asset.id, true, true)
      }
    })
  }, [assets, selection.selectedIds, clearSelection, selectAsset])

  // 获取选择相关的键盘事件处理器
  const getKeyboardHandlers = useCallback(
    (columnsPerRow?: number) => ({
      'Ctrl+a': (e: KeyboardEvent) => {
        e.preventDefault()
        toggleSelectAll()
      },
      Escape: () => {
        clearSelection()
        setActiveAsset(undefined)
      },
      Space: (e: KeyboardEvent) => {
        e.preventDefault()
        selectActiveAsset()
      },
      ArrowLeft: (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('left', columnsPerRow)
      },
      ArrowRight: (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('right', columnsPerRow)
      },
      ArrowUp: (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('up', columnsPerRow)
      },
      ArrowDown: (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('down', columnsPerRow)
      },
      'Shift+ArrowLeft': (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('left', columnsPerRow)
        selectActiveAsset()
      },
      'Shift+ArrowRight': (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('right', columnsPerRow)
        selectActiveAsset()
      },
      'Shift+ArrowUp': (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('up', columnsPerRow)
        selectActiveAsset()
      },
      'Shift+ArrowDown': (e: KeyboardEvent) => {
        e.preventDefault()
        navigateSelection('down', columnsPerRow)
        selectActiveAsset()
      },
    }),
    [toggleSelectAll, clearSelection, setActiveAsset, selectActiveAsset, navigateSelection]
  )

  return {
    // 状态
    selection,
    selectedAssets,
    selectedCount,
    isAllSelected,
    isPartiallySelected,
    activeAsset: assets.find((asset) => asset.id === selection.activeId),

    // 基础操作
    handleAssetClick,
    handleAssetSelect,
    toggleAssetSelection,
    toggleSelectAll,
    selectRange,
    clearSelection,

    // 高级选择
    selectByType,
    selectByCondition,
    invertSelection,

    // 键盘导航
    navigateSelection,
    selectActiveAsset,
    getKeyboardHandlers,

    // 便捷方法
    selectAllPhotos: () => selectByType('photo'),
    selectAllVideos: () => selectByType('video'),
    selectAllLivePhotos: () => selectByType('live_photo'),
    hasSelection: selectedCount > 0,
  }
}
