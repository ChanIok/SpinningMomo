import { reactive, computed, ref, type Ref } from 'vue'
import type {
  Asset,
  SelectionState,
  LightboxState,
  DetailsPanelFocus,
  BatchSelectionSummary,
} from '../types'
import type { GalleryInputType } from '../input'
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
    mode: 'browse',
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
    inputType: 'mouse',
    isImmersive: false,
    chromeVisible: true,
    showFilmstrip: true,
    zoom: 1.0,
    fitMode: 'contain',
    rotationDegrees: 0,
  })

  // detailsPanel 只保存右侧详情的焦点身份，不复制资产/树节点数据。
  const detailsPanel = ref<DetailsPanelFocus>({ type: 'none' })

  const selectedCount = computed(() => selection.selectedIds.size)
  const hasSelection = computed(() => selectedCount.value > 0)
  const batchSummary = ref<BatchSelectionSummary | null>(null)
  const batchSummaryLoading = ref(false)
  const batchSummaryRequestVersion = ref(0)

  function patchAssetsReviewState(
    assetIds: number[],
    updates: Partial<Pick<Asset, 'rating' | 'reviewFlag'>>
  ) {
    if (assetIds.length === 0) {
      return
    }

    const assetIdSet = new Set(assetIds)

    // 审片操作是高频交互，这里只 patch 当前已加载页面，避免每次按键都整页重载。
    // 注意：这里只保证“已加载页”即时一致，其余页由后续查询刷新补齐。
    const nextPages = new Map(paginatedAssets.value)
    let hasCacheChange = false

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
        nextPages.set(pageNum, nextPageAssets)
        hasCacheChange = true
      }
    })

    if (hasCacheChange) {
      paginatedAssets.value = nextPages
      bumpPaginatedAssetsVersion()
    }
  }

  function patchAssetDescription(assetId: number, description?: string) {
    const nextPages = new Map(paginatedAssets.value)
    let hasCacheChange = false

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
        nextPages.set(pageNum, nextPageAssets)
        hasCacheChange = true
      }
    })

    if (hasCacheChange) {
      paginatedAssets.value = nextPages
      bumpPaginatedAssetsVersion()
    }
  }

  function patchAssetsDescription(assetIds: number[], description?: string) {
    if (assetIds.length === 0) {
      return
    }

    const assetIdSet = new Set(assetIds)
    const nextPages = new Map(paginatedAssets.value)
    let hasCacheChange = false

    paginatedAssets.value.forEach((pageAssets, pageNum) => {
      let hasPageChange = false
      const nextPageAssets = pageAssets.map((asset) => {
        if (!assetIdSet.has(asset.id)) {
          return asset
        }

        hasPageChange = true
        return {
          ...asset,
          description,
        }
      })

      if (hasPageChange) {
        nextPages.set(pageNum, nextPageAssets)
        hasCacheChange = true
      }
    })

    if (hasCacheChange) {
      paginatedAssets.value = nextPages
      bumpPaginatedAssetsVersion()
    }
  }

  function beginBatchSummaryRefresh(): number {
    // 批量摘要跟随选择集变化很频繁；每次开始新请求都先清空旧摘要，
    // 避免用户在快速切换选择时看到过期的“公共评分/公共标签”。
    batchSummaryRequestVersion.value += 1
    batchSummaryLoading.value = true
    batchSummary.value = null
    return batchSummaryRequestVersion.value
  }

  function finishBatchSummaryRefresh(version: number) {
    if (batchSummaryRequestVersion.value === version) {
      batchSummaryLoading.value = false
    }
  }

  function isBatchSummaryRequestCurrent(version: number): boolean {
    return batchSummaryRequestVersion.value === version
  }

  function setBatchSummary(summary: BatchSelectionSummary | null) {
    batchSummary.value = summary
  }

  function resetBatchSummary() {
    batchSummaryRequestVersion.value += 1
    batchSummaryLoading.value = false
    batchSummary.value = null
  }

  function patchBatchSummaryReviewState(updates: Partial<Pick<Asset, 'rating' | 'reviewFlag'>>) {
    if (detailsPanel.value.type !== 'batch' || batchSummary.value === null) {
      return
    }

    // 批量评分/弃置成功后先同步摘要层，避免等异步重查期间右侧面板短暂回弹到旧状态。
    batchSummary.value = {
      ...batchSummary.value,
      ...(updates.rating !== undefined ? { rating: updates.rating } : {}),
      ...(updates.reviewFlag !== undefined
        ? { rejectedState: updates.reviewFlag === 'rejected' }
        : {}),
    }
  }

  function patchBatchSummaryDescription(description?: string) {
    if (detailsPanel.value.type !== 'batch' || batchSummary.value === null) {
      return
    }

    batchSummary.value = {
      ...batchSummary.value,
      description: description ?? '',
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

  function enterMultiSelectMode() {
    selection.mode = 'multi-select'
  }

  function exitMultiSelectMode() {
    selection.mode = 'browse'
    selection.selectedIds.clear()
    selection.anchorIndex = undefined
    clearActiveAsset()
    clearDetailsFocus()
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

  function openLightbox(inputType: GalleryInputType = 'mouse') {
    resetLightboxView()
    lightbox.inputType = inputType
    lightbox.isClosing = false
    lightbox.chromeVisible = true
    lightbox.isOpen = true
  }

  function setLightboxClosing(closing: boolean) {
    lightbox.isClosing = closing
  }

  function closeLightbox() {
    lightbox.isOpen = false
    lightbox.isClosing = false
    lightbox.isImmersive = false
    lightbox.chromeVisible = true
    lightbox.inputType = 'mouse'
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

  function setLightboxChromeVisible(visible: boolean) {
    lightbox.chromeVisible = visible
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
    detailsPanel.value = focus
  }

  function clearDetailsFocus() {
    detailsPanel.value = { type: 'none' }
  }

  function resetInteractionState() {
    // 只重置交互域。query/navigation 的 reset 由主入口统一调度。
    selection.mode = 'browse'
    selection.selectedIds.clear()
    selection.anchorIndex = undefined
    selection.activeIndex = undefined
    selection.activeAssetId = undefined

    lightbox.isOpen = false
    lightbox.isClosing = false
    lightbox.isImmersive = false
    lightbox.chromeVisible = true
    lightbox.inputType = 'mouse'
    lightbox.showFilmstrip = true
    resetLightboxView()

    resetBatchSummary()
    clearDetailsFocus()
  }

  return {
    selection,
    lightbox,
    detailsPanel,
    selectedCount,
    hasSelection,
    batchSummary,
    batchSummaryLoading,
    batchSummaryRequestVersion,
    patchAssetsReviewState,
    patchAssetDescription,
    patchAssetsDescription,
    patchBatchSummaryReviewState,
    patchBatchSummaryDescription,
    beginBatchSummaryRefresh,
    finishBatchSummaryRefresh,
    isBatchSummaryRequestCurrent,
    setBatchSummary,
    resetBatchSummary,
    selectAsset,
    enterMultiSelectMode,
    exitMultiSelectMode,
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
    setLightboxChromeVisible,
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
