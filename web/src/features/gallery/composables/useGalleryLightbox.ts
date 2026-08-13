import { useGalleryStore } from '../store'
import { useGallerySelection } from './useGallerySelection'
import type { GalleryInputType } from '../input'

export function useGalleryLightbox() {
  const store = useGalleryStore()
  const gallerySelection = useGallerySelection()
  async function syncLightboxSelection(index: number) {
    if (store.selectedCount > 1) {
      return gallerySelection.activateIndex(index, { syncDetails: true })
    }

    return gallerySelection.selectOnlyIndex(index)
  }

  async function openLightbox(index: number, inputType: GalleryInputType = 'mouse') {
    const asset = await syncLightboxSelection(index)
    if (!asset) {
      return
    }

    store.openLightbox(inputType)
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

  function closeLightbox() {
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
