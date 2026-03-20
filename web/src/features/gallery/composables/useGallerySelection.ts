import { computed } from 'vue'
import { useGalleryData } from './useGalleryData'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'

/**
 * Gallery选择管理 Composable
 * 负责资产的选择交互逻辑：单选、多选、范围选择等
 */
export function useGallerySelection() {
  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const selectedIds = computed(() => store.selection.selectedIds)
  const selectedCount = computed(() => store.selectedCount)
  const hasSelection = computed(() => store.hasSelection)
  const activeIndex = computed(() => store.selection.activeIndex)
  const activeAssetId = computed(() => store.selection.activeAssetId)
  const totalCount = computed(() => store.totalCount)
  const perPage = computed(() => store.perPage)

  function clearSelection() {
    store.clearSelection()
    store.clearDetailsFocus()
  }

  function normalizeIndex(index: number): number | undefined {
    if (totalCount.value <= 0) {
      return undefined
    }

    return Math.max(0, Math.min(index, totalCount.value - 1))
  }

  function findLoadedAssetById(id: number): Asset | undefined {
    for (const pageAssets of store.paginatedAssets.values()) {
      const asset = pageAssets.find((item) => item.id === id)
      if (asset) {
        return asset
      }
    }

    return undefined
  }

  function getLoadedAssetByIndex(index: number): Asset | undefined {
    const [asset] = store.getAssetsInRange(index, index)
    return asset ?? undefined
  }

  async function ensureRangeLoaded(startIndex: number, endIndex: number) {
    const startPage = Math.floor(startIndex / perPage.value) + 1
    const endPage = Math.floor(endIndex / perPage.value) + 1
    const loadPromises: Promise<void>[] = []

    for (let page = startPage; page <= endPage; page++) {
      if (!store.isPageLoaded(page)) {
        loadPromises.push(galleryData.loadPage(page))
      }
    }

    if (loadPromises.length > 0) {
      await Promise.all(loadPromises)
    }
  }

  async function getAssetByIndex(index: number): Promise<Asset | undefined> {
    const normalizedIndex = normalizeIndex(index)
    if (normalizedIndex === undefined) {
      return undefined
    }

    await ensureRangeLoaded(normalizedIndex, normalizedIndex)
    return getLoadedAssetByIndex(normalizedIndex)
  }

  function getPrimarySelectedAsset(preferredAsset?: Asset): Asset | undefined {
    if (preferredAsset && selectedIds.value.has(preferredAsset.id)) {
      return preferredAsset
    }

    const currentActiveAssetId = activeAssetId.value
    if (currentActiveAssetId !== undefined) {
      const activeAsset = findLoadedAssetById(currentActiveAssetId)
      if (activeAsset && selectedIds.value.has(activeAsset.id)) {
        return activeAsset
      }
    }

    for (const id of selectedIds.value) {
      const asset = findLoadedAssetById(id)
      if (asset) {
        return asset
      }
    }

    return undefined
  }

  function syncDetailsFocusFromSelection(preferredAsset?: Asset) {
    if (store.selectedCount > 1) {
      store.setDetailsFocus({ type: 'batch' })
      return
    }

    if (store.selectedCount === 1) {
      const asset = getPrimarySelectedAsset(preferredAsset)
      if (asset) {
        store.setDetailsFocus({ type: 'asset', asset })
        return
      }
    }

    store.clearDetailsFocus()
  }

  async function activateIndex(index: number, options: { syncDetails?: boolean } = {}) {
    const normalizedIndex = normalizeIndex(index)
    if (normalizedIndex === undefined) {
      return undefined
    }

    const asset = await getAssetByIndex(normalizedIndex)
    if (!asset) {
      return undefined
    }

    store.setActiveAsset(asset.id, normalizedIndex)
    if (options.syncDetails) {
      syncDetailsFocusFromSelection(asset)
    }

    return asset
  }

  async function selectOnlyIndex(index: number) {
    const normalizedIndex = normalizeIndex(index)
    if (normalizedIndex === undefined) {
      return undefined
    }

    const asset = await getAssetByIndex(normalizedIndex)
    if (!asset) {
      return undefined
    }

    store.replaceSelection([asset.id])
    store.setSelectionAnchor(normalizedIndex)
    store.setActiveAsset(asset.id, normalizedIndex)
    syncDetailsFocusFromSelection(asset)
    return asset
  }

  async function toggleIndex(index: number) {
    const normalizedIndex = normalizeIndex(index)
    if (normalizedIndex === undefined) {
      return undefined
    }

    const asset = await getAssetByIndex(normalizedIndex)
    if (!asset) {
      return undefined
    }

    const wasSelected = selectedIds.value.has(asset.id)
    store.selectAsset(asset.id, !wasSelected, true)

    if (!wasSelected) {
      store.setSelectionAnchor(normalizedIndex)
    } else if (store.selectedCount === 0) {
      store.setSelectionAnchor(undefined)
    }

    store.setActiveAsset(asset.id, normalizedIndex)
    syncDetailsFocusFromSelection(asset)
    return asset
  }

  async function rangeSelectToIndex(index: number) {
    const normalizedIndex = normalizeIndex(index)
    if (normalizedIndex === undefined) {
      return undefined
    }

    const anchorIndex = store.selection.anchorIndex
    if (anchorIndex === undefined || anchorIndex < 0 || anchorIndex >= totalCount.value) {
      return selectOnlyIndex(normalizedIndex)
    }

    const startIndex = Math.min(anchorIndex, normalizedIndex)
    const endIndex = Math.max(anchorIndex, normalizedIndex)
    await ensureRangeLoaded(startIndex, endIndex)

    const selectedRangeIds: number[] = []
    for (let currentIndex = startIndex; currentIndex <= endIndex; currentIndex++) {
      const asset = getLoadedAssetByIndex(currentIndex)
      if (asset) {
        selectedRangeIds.push(asset.id)
      }
    }

    if (selectedRangeIds.length === 0) {
      return undefined
    }

    const targetAsset = getLoadedAssetByIndex(normalizedIndex)
    store.replaceSelection(selectedRangeIds)
    if (targetAsset) {
      store.setActiveAsset(targetAsset.id, normalizedIndex)
    } else {
      store.setSelectionActive(normalizedIndex)
    }
    syncDetailsFocusFromSelection(targetAsset)
    return targetAsset
  }

  async function handleAssetClick(_asset: Asset, event: MouseEvent, index: number) {
    if (event.shiftKey) {
      await rangeSelectToIndex(index)
      return
    }

    if (event.ctrlKey || event.metaKey) {
      await toggleIndex(index)
      return
    }

    await selectOnlyIndex(index)
  }

  function handleAssetDoubleClick(_asset: Asset, _event: MouseEvent) {
    // The view layer decides what double click should do (for example, open the Lightbox).
  }

  async function prepareContextMenuForIndex(index: number) {
    const normalizedIndex = normalizeIndex(index)
    if (normalizedIndex === undefined) {
      return undefined
    }

    const asset = await getAssetByIndex(normalizedIndex)
    if (!asset) {
      return undefined
    }

    if (selectedIds.value.has(asset.id)) {
      store.setActiveAsset(asset.id, normalizedIndex)
      syncDetailsFocusFromSelection(asset)
      return asset
    }

    return selectOnlyIndex(normalizedIndex)
  }

  async function handleAssetContextMenu(asset: Asset, _event: MouseEvent, index?: number) {
    if (index !== undefined) {
      return prepareContextMenuForIndex(index)
    }

    if (!selectedIds.value.has(asset.id)) {
      store.replaceSelection([asset.id])
      store.setActiveAssetId(asset.id)
      syncDetailsFocusFromSelection(asset)
      return asset
    }

    store.setActiveAssetId(asset.id)
    syncDetailsFocusFromSelection(asset)
    return asset
  }

  function isAssetSelected(id: number): boolean {
    return selectedIds.value.has(id)
  }

  return {
    selectedIds,
    selectedCount,
    hasSelection,
    activeIndex,
    activeAssetId,

    clearSelection,
    activateIndex,
    selectOnlyIndex,
    toggleIndex,
    rangeSelectToIndex,
    syncDetailsFocusFromSelection,
    prepareContextMenuForIndex,

    handleAssetClick,
    handleAssetDoubleClick,
    handleAssetContextMenu,

    isAssetSelected,
  }
}
