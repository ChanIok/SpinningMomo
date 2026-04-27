import { reactive, computed, type Ref } from 'vue'
import type { Asset, SelectionState, LightboxState, DetailsPanelFocus } from '../types'
import { LIGHTBOX_MAX_ZOOM, LIGHTBOX_MIN_ZOOM } from './persistence'

interface InteractionSliceArgs {
  // interaction 依赖 query 结果集做局部 patch 与 lightbox 边界裁剪。
  totalCount: Ref<number>
  paginatedAssets: Ref<Map<number, Asset[]>>
  // 由 index 注入，确保 interaction 修改缓存后能触发观察者更新。
  bumpPaginatedAssetsVersion: () => void
}

/**
 * Interaction Slice
 *
 * 关注点:
 * - 用户交互态：selection / lightbox / details focus
 * - 与交互强耦合的“本地即时 patch”（评分、描述）
 */
export function createInteractionSlice(args: InteractionSliceArgs) {
  const { totalCount, paginatedAssets, bumpPaginatedAssetsVersion } = args

  // selection 的语义分层：
  // - selectedIds: 多选集合
  // - anchorIndex: 范围选择锚点
  // - activeIndex: 当前结果集位置（可能随查询变化失效）
  // - activeAssetId: 当前聚焦资产身份（跨查询变化保持语义）
  const selection = reactive<SelectionState>({
    selectedIds: new Set<number>(),
    anchorIndex: undefined,
    // activeIndex 是当前结果集里的位置缓存；筛选/排序变化后可能失效，需要重定位。
    activeIndex: undefined,
    // activeAssetId 才是“当前聚焦资产”的身份真相源，用来跨结果集变化保留语义。
    activeAssetId: undefined,
  })

  // lightbox 只保存“展示控制态”，真实资产数据仍来自 query 缓存。
  const lightbox = reactive<LightboxState>({
    isOpen: false,
    isClosing: false,
    isImmersive: false,
    showFilmstrip: true,
    zoom: 1.0,
    fitMode: 'contain',
    rotationDegrees: 0,
  })

  // detailsPanel 是右侧详情“当前焦点类型”的真相源。
  const detailsPanel: DetailsPanelFocus = reactive<DetailsPanelFocus>({
    type: 'none',
  })

  const selectedCount = computed(() => selection.selectedIds.size)
  const hasSelection = computed(() => selectedCount.value > 0)

  function patchAssetsReviewState(
    assetIds: number[],
    updates: Partial<Pick<Asset, 'rating' | 'reviewFlag'>>
  ) {
    if (assetIds.length === 0) {
      return
    }

    const assetIdSet = new Set(assetIds)

    // 审片操作是高频交互，这里直接原地 patch 当前已加载页面，避免每次按键都整页重载。
    // 注意：这里只保证“已加载页”即时一致，其余页由后续查询刷新补齐。
    paginatedAssets.value.forEach((pageAssets, pageNum) => {
      let hasPageChange = false
      const nextPageAssets = pageAssets.map((asset) => {
        if (!assetIdSet.has(asset.id)) {
          return asset
        }

        hasPageChange = true
        return {
          ...asset,
          ...(updates.rating !== undefined ? { rating: updates.rating } : {}),
          ...(updates.reviewFlag !== undefined ? { reviewFlag: updates.reviewFlag } : {}),
        }
      })

      if (hasPageChange) {
        paginatedAssets.value.set(pageNum, nextPageAssets)
        // Map 原地更新后手动 bump，确保依赖 paginatedAssetsVersion 的渲染及时更新。
        bumpPaginatedAssetsVersion()
      }
    })

    // 详情面板若正聚焦某个被 patch 的资产，也要同步更新，避免左右视图状态分叉。
    if (detailsPanel.type === 'asset' && assetIdSet.has(detailsPanel.asset.id)) {
      detailsPanel.asset = {
        ...detailsPanel.asset,
        ...(updates.rating !== undefined ? { rating: updates.rating } : {}),
        ...(updates.reviewFlag !== undefined ? { reviewFlag: updates.reviewFlag } : {}),
      }
    }
  }

  function patchAssetDescription(assetId: number, description?: string) {
    paginatedAssets.value.forEach((pageAssets, pageNum) => {
      let hasPageChange = false
      const nextPageAssets = pageAssets.map((asset) => {
        if (asset.id !== assetId) {
          return asset
        }

        hasPageChange = true
        return {
          ...asset,
          description,
        }
      })

      if (hasPageChange) {
        paginatedAssets.value.set(pageNum, nextPageAssets)
        bumpPaginatedAssetsVersion()
      }
    })

    if (detailsPanel.type === 'asset' && detailsPanel.asset.id === assetId) {
      detailsPanel.asset = {
        ...detailsPanel.asset,
        description,
      }
    }
  }

  function selectAsset(id: number, selected: boolean, multi = false) {
    // 单选默认清空旧选中；多选由调用方显式传 multi=true。
    if (!multi) {
      selection.selectedIds.clear()
    }

    if (selected) {
      selection.selectedIds.add(id)
    } else {
      selection.selectedIds.delete(id)
    }
  }

  function clearSelection() {
    selection.selectedIds.clear()
    selection.anchorIndex = undefined
  }

  function replaceSelection(ids: number[]) {
    selection.selectedIds.clear()
    ids.forEach((id) => selection.selectedIds.add(id))
  }

  function setSelectionAnchor(index?: number) {
    selection.anchorIndex = index
  }

  function setSelectionActive(index?: number) {
    selection.activeIndex = index
  }

  function setActiveAsset(assetId: number, index?: number) {
    // 始终同时更新 identity 与 position，降低调用方维护一致性的负担。
    selection.activeAssetId = assetId
    selection.activeIndex = index
  }

  function setActiveAssetId(assetId?: number) {
    selection.activeAssetId = assetId
  }

  function clearActiveAsset() {
    selection.activeAssetId = undefined
    selection.activeIndex = undefined
  }

  function resetLightboxView() {
    // 只重置展示控制态，不改变 open/close 状态。
    lightbox.zoom = 1.0
    lightbox.fitMode = 'contain'
    lightbox.rotationDegrees = 0
  }

  function openLightbox() {
    resetLightboxView()
    lightbox.isClosing = false
    lightbox.isOpen = true
  }

  function setLightboxClosing(closing: boolean) {
    lightbox.isClosing = closing
  }

  function closeLightbox() {
    lightbox.isOpen = false
    lightbox.isClosing = false
    lightbox.isImmersive = false
    resetLightboxView()
  }

  function goToLightboxIndex(index: number) {
    if (lightbox.isOpen) {
      // 统一做边界裁剪，防止调用方传入越界索引。
      const validIndex = Math.max(0, Math.min(index, totalCount.value - 1))
      selection.activeIndex = validIndex
    }
  }

  function goToPreviousLightbox() {
    const currentIndex = selection.activeIndex ?? 0
    if (lightbox.isOpen && currentIndex > 0) {
      selection.activeIndex = currentIndex - 1
    }
  }

  function goToNextLightbox() {
    const currentIndex = selection.activeIndex ?? 0
    if (lightbox.isOpen && currentIndex < totalCount.value - 1) {
      selection.activeIndex = currentIndex + 1
    }
  }

  function setLightboxImmersive(immersive: boolean) {
    lightbox.isImmersive = immersive
  }

  function toggleLightboxImmersive() {
    setLightboxImmersive(!lightbox.isImmersive)
  }

  function toggleLightboxFilmstrip() {
    lightbox.showFilmstrip = !lightbox.showFilmstrip
  }

  function setLightboxZoom(zoom: number) {
    lightbox.zoom = Math.max(LIGHTBOX_MIN_ZOOM, Math.min(LIGHTBOX_MAX_ZOOM, zoom))
  }

  function setLightboxFitMode(mode: LightboxState['fitMode']) {
    lightbox.fitMode = mode
  }

  function normalizeLightboxRotation(degrees: number) {
    return ((degrees % 360) + 360) % 360
  }

  function rotateLightboxView(deltaDegrees: number) {
    lightbox.rotationDegrees = normalizeLightboxRotation(lightbox.rotationDegrees + deltaDegrees)
  }

  function setDetailsFocus(focus: DetailsPanelFocus) {
    // 用 assign 保持 reactive 对象引用不变，减少依赖断联风险。
    Object.assign(detailsPanel, focus)
  }

  function clearDetailsFocus() {
    detailsPanel.type = 'none'
  }

  function resetInteractionState() {
    // 只重置交互域。query/navigation 的 reset 由主入口统一调度。
    selection.selectedIds.clear()
    selection.anchorIndex = undefined
    selection.activeIndex = undefined
    selection.activeAssetId = undefined

    lightbox.isOpen = false
    lightbox.isClosing = false
    lightbox.isImmersive = false
    lightbox.showFilmstrip = true
    resetLightboxView()

    clearDetailsFocus()
  }

  return {
    selection,
    lightbox,
    detailsPanel,
    selectedCount,
    hasSelection,
    patchAssetsReviewState,
    patchAssetDescription,
    selectAsset,
    clearSelection,
    replaceSelection,
    setSelectionAnchor,
    setSelectionActive,
    setActiveAsset,
    setActiveAssetId,
    clearActiveAsset,
    resetLightboxView,
    openLightbox,
    setLightboxClosing,
    closeLightbox,
    goToLightboxIndex,
    goToPreviousLightbox,
    goToNextLightbox,
    setLightboxImmersive,
    toggleLightboxImmersive,
    toggleLightboxFilmstrip,
    setLightboxZoom,
    setLightboxFitMode,
    rotateLightboxView,
    setDetailsFocus,
    clearDetailsFocus,
    resetInteractionState,
  }
}
