import { ref, watch } from 'vue'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'

interface ImageState {
  status: 'idle' | 'loading' | 'loaded' | 'error'
  url?: string
}

export function useGalleryLightbox() {
  const store = useGalleryStore()

  const imageStates = ref<Map<number, ImageState>>(new Map())
  const loading = ref<Set<number>>(new Set())
  const loaded = ref<Set<number>>(new Set())

  /**
   * 预加载单张图片
   * 使用 new Image() 而非 fetch，是为了让浏览器将图片写入 HTTP 缓存，
   * 后续 <img> 标签请求同一 URL 时可直接命中缓存，无需二次下载。
   */
  async function preloadImage(assetId: number): Promise<void> {
    if (loaded.value.has(assetId) || loading.value.has(assetId)) {
      return
    }

    loading.value.add(assetId)
    const url = galleryApi.getAssetUrl(assetId)
    imageStates.value.set(assetId, { status: 'loading', url })

    return new Promise((resolve, reject) => {
      const img = new Image()
      img.onload = () => {
        loading.value.delete(assetId)
        loaded.value.add(assetId)
        imageStates.value.set(assetId, { status: 'loaded', url })
        resolve()
      }
      img.onerror = () => {
        loading.value.delete(assetId)
        imageStates.value.set(assetId, { status: 'error', url })
        reject(new Error(`Failed to load image: ${assetId}`))
      }
      img.src = url
    })
  }

  /**
   * 预加载当前图片及前后各 PRELOAD_RANGE 张
   * 策略：优先 await 当前帧确保尽快显示，再并行预加载相邻帧
   */
  async function preloadRange(currentIndex: number) {
    const PRELOAD_RANGE = 2
    const totalCount = store.totalCount
    const start = Math.max(0, currentIndex - PRELOAD_RANGE)
    const end = Math.min(totalCount - 1, currentIndex + PRELOAD_RANGE)

    const currentAsset = store.getAssetsInRange(currentIndex, currentIndex)[0]
    if (!currentAsset) return

    try {
      await preloadImage(currentAsset.id)
    } catch (err) {
      console.warn(
        `Failed to preload current image [index=${currentIndex}, id=${currentAsset.id}]`,
        err
      )
    }

    const preloadPromises: Promise<void>[] = []
    for (let offset = 1; offset <= PRELOAD_RANGE; offset++) {
      if (currentIndex + offset <= end) {
        const asset = store.getAssetsInRange(currentIndex + offset, currentIndex + offset)[0]
        if (asset) {
          preloadPromises.push(
            preloadImage(asset.id).catch((err) => {
              console.warn(
                `Failed to preload image [index=${currentIndex + offset}, id=${asset.id}]`,
                err
              )
            })
          )
        }
      }

      if (currentIndex - offset >= start) {
        const asset = store.getAssetsInRange(currentIndex - offset, currentIndex - offset)[0]
        if (asset) {
          preloadPromises.push(
            preloadImage(asset.id).catch((err) => {
              console.warn(
                `Failed to preload image [index=${currentIndex - offset}, id=${asset.id}]`,
                err
              )
            })
          )
        }
      }
    }

    Promise.allSettled(preloadPromises)
  }

  function getImageState(assetId: number): ImageState {
    return imageStates.value.get(assetId) || { status: 'idle' }
  }

  watch(
    () => store.lightbox.currentIndex,
    (newIndex) => {
      if (store.lightbox.isOpen) {
        preloadRange(newIndex).catch((err) => {
          console.warn('Failed to preload lightbox range:', err)
        })
      }
    },
    { immediate: true }
  )

  function openLightbox(index: number) {
    store.openLightbox(index)
    preloadRange(index).catch((err) => {
      console.warn('Failed to preload lightbox range:', err)
    })
  }

  function setFullscreen(fullscreen: boolean) {
    store.setLightboxFullscreen(fullscreen)
  }

  function toggleFullscreen() {
    store.toggleLightboxFullscreen()
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

  function toggleFitActual() {
    if (store.lightbox.fitMode === 'contain') {
      showActualSize()
      return
    }

    showFitMode()
  }

  function goToPrevious() {
    store.goToPreviousLightbox()
  }

  function goToNext() {
    store.goToNextLightbox()
  }

  function goToIndex(index: number) {
    store.goToLightboxIndex(index)
  }

  function closeLightbox() {
    store.closeLightbox()
  }

  return {
    openLightbox,
    closeLightbox,
    setFullscreen,
    toggleFullscreen,
    toggleFilmstrip,
    showFitMode,
    showActualSize,
    setActualZoom,
    toggleFitActual,
    goToPrevious,
    goToNext,
    goToIndex,
    getImageState,
    preloadImage,
  }
}
