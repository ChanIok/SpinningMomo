import { useGalleryStore } from '../store'
import { useGallerySelection } from './useGallerySelection'
import { isGalleryLightboxOverlay, useGalleryOverlayHistory } from './useGalleryOverlayHistory'
import type { GalleryInputType } from '../input'

// 协调暗房的 Store 状态与 URL 历史，保证打开、切图和关闭由同一条状态链驱动。
export function useGalleryLightbox() {
  const store = useGalleryStore()
  const gallerySelection = useGallerySelection()
  const overlayHistory = useGalleryOverlayHistory()
  async function syncLightboxSelection(index: number) {
    if (store.selectedCount > 1) {
      return gallerySelection.activateIndex(index, { syncDetails: true })
    }

    return gallerySelection.selectOnlyIndex(index)
  }

  // 先同步选中资产，再同时打开暗房并写入可返回的历史层。
  async function openLightbox(index: number, inputType: GalleryInputType = 'mouse') {
    const asset = await syncLightboxSelection(index)
    if (!asset) {
      return
    }

    store.openLightbox(inputType)
    try {
      // 历史写入失败时回滚 Store，避免出现看得见暗房但无法正常返回的半状态。
      await overlayHistory.openLightbox(asset.id)
    } catch (error) {
      console.error('Failed to record lightbox history entry:', error)
      store.closeLightbox()
    }
  }

  function setImmersive(immersive: boolean) {
    store.setLightboxImmersive(immersive)
  }

  function toggleImmersive() {
    store.toggleLightboxImmersive()
  }

  function toggleFilmstrip() {
    store.toggleLightboxFilmstrip()
  }

  function showFitMode() {
    store.setLightboxFitMode('contain')
  }

  function showActualSize() {
    store.setLightboxFitMode('actual')
    store.setLightboxZoom(1)
  }

  function setActualZoom(zoom: number) {
    store.setLightboxFitMode('actual')
    store.setLightboxZoom(zoom)
  }

  function rotateView(deltaDegrees: number) {
    store.rotateLightboxView(deltaDegrees)
  }

  function toggleFitActual() {
    if (store.lightbox.fitMode === 'contain') {
      showActualSize()
      return
    }

    showFitMode()
  }

  function goToPrevious() {
    const currentIndex = store.selection.activeIndex
    if (currentIndex === undefined || currentIndex <= 0) {
      return
    }

    void syncLightboxSelection(currentIndex - 1)
  }

  function goToNext() {
    const currentIndex = store.selection.activeIndex
    if (currentIndex === undefined || currentIndex >= store.totalCount - 1) {
      return
    }

    void syncLightboxSelection(currentIndex + 1)
  }

  function goToIndex(index: number) {
    void syncLightboxSelection(index)
  }

  // 有覆盖层历史时通过回退关闭；没有历史时保留 Store 兜底，兼容内部临时状态。
  function closeLightbox() {
    if (isGalleryLightboxOverlay(overlayHistory.snapshot.value.overlay)) {
      void overlayHistory.closeLightbox()
      return
    }

    store.closeLightbox()
  }

  return {
    openLightbox,
    closeLightbox,
    setImmersive,
    toggleImmersive,
    toggleFilmstrip,
    showFitMode,
    showActualSize,
    setActualZoom,
    rotateView,
    toggleFitActual,
    goToPrevious,
    goToNext,
    goToIndex,
  }
}
