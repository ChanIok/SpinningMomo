import { useGalleryStore } from '../store'
import { galleryApi } from '../api'
import { ref, watch } from 'vue'

/**
 * 图片加载状态
 */
interface ImageState {
  status: 'idle' | 'loading' | 'loaded' | 'error'
  url?: string
}

/**
 * Gallery Lightbox Composable
 * 负责lightbox的业务逻辑：打开/关闭、导航、图片预加载等
 */
export function useGalleryLightbox() {
  const store = useGalleryStore()

  // 图片加载状态管理
  const imageStates = ref<Map<number, ImageState>>(new Map())
  const loading = ref<Set<number>>(new Set())
  const loaded = ref<Set<number>>(new Set())

  /**
   * 预加载单张图片
   */
  async function preloadImage(assetId: number): Promise<void> {
    // 已加载或正在加载，跳过
    if (loaded.value.has(assetId) || loading.value.has(assetId)) {
      return
    }

    loading.value.add(assetId)
    const url = galleryApi.getAssetUrl(assetId)

    // 更新状态为加载中
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
   * 预加载当前索引前后的图片
   * 先加载当前图片，再预加载周围图片
   */
  async function preloadRange(currentIndex: number) {
    const PRELOAD_RANGE = 2 // 前后各2张
    const totalCount = store.totalCount

    // 计算预加载范围
    const start = Math.max(0, currentIndex - PRELOAD_RANGE)
    const end = Math.min(totalCount - 1, currentIndex + PRELOAD_RANGE)

    // 1. 先获取当前图片资源
    const currentAsset = store.getAssetsInRange(currentIndex, currentIndex)[0]
    if (!currentAsset) return

    // 2. 等待当前图片加载完成（优先级最高）
    try {
      await preloadImage(currentAsset.id)
    } catch (err) {
      console.warn(`当前图片加载失败 [index=${currentIndex}, id=${currentAsset.id}]:`, err)
    }

    // 3. 当前图片加载完成后，并发预加载周围图片
    const preloadPromises: Promise<void>[] = []
    for (let offset = 1; offset <= PRELOAD_RANGE; offset++) {
      // 下一张
      if (currentIndex + offset <= end) {
        const asset = store.getAssetsInRange(currentIndex + offset, currentIndex + offset)[0]
        if (asset) {
          preloadPromises.push(
            preloadImage(asset.id).catch((err) => {
              console.warn(`预加载图片失败 [index=${currentIndex + offset}, id=${asset.id}]:`, err)
            })
          )
        }
      }

      // 上一张
      if (currentIndex - offset >= start) {
        const asset = store.getAssetsInRange(currentIndex - offset, currentIndex - offset)[0]
        if (asset) {
          preloadPromises.push(
            preloadImage(asset.id).catch((err) => {
              console.warn(`预加载图片失败 [index=${currentIndex - offset}, id=${asset.id}]:`, err)
            })
          )
        }
      }
    }

    // 并发执行预加载（不等待）
    Promise.allSettled(preloadPromises)
  }

  /**
   * 获取图片加载状态
   */
  function getImageState(assetId: number): ImageState {
    return imageStates.value.get(assetId) || { status: 'idle' }
  }

  /**
   * 监听 currentIndex 变化，自动预加载
   */
  watch(
    () => store.lightbox.currentIndex,
    (newIndex) => {
      if (store.lightbox.isOpen) {
        preloadRange(newIndex).catch((err) => {
          console.warn('预加载图片失败:', err)
        })
      }
    },
    { immediate: true }
  )

  /**
   * 打开Lightbox
   * @param index 要打开的资产的全局索引
   */
  function openLightbox(index: number) {
    store.openLightbox(index)
    // 打开时立即预加载
    preloadRange(index).catch((err) => {
      console.warn('预加载图片失败:', err)
    })
  }

  /**
   * 切换全屏模式
   */
  function toggleFullscreen() {
    if (!store.lightbox.isFullscreen) {
      // 进入全屏
      document.documentElement.requestFullscreen?.()
    } else {
      // 退出全屏
      document.exitFullscreen?.()
    }
    store.toggleLightboxFullscreen()
  }

  /**
   * 切换Filmstrip显示
   */
  function toggleFilmstrip() {
    store.toggleLightboxFilmstrip()
  }

  /**
   * 导航到上一张
   */
  function goToPrevious() {
    store.goToPreviousLightbox()
  }

  /**
   * 导航到下一张
   */
  function goToNext() {
    store.goToNextLightbox()
  }

  /**
   * 跳转到指定索引
   */
  function goToIndex(index: number) {
    store.goToLightboxIndex(index)
  }

  /**
   * 关闭Lightbox
   */
  function closeLightbox() {
    store.closeLightbox()
  }

  return {
    openLightbox,
    closeLightbox,
    toggleFullscreen,
    toggleFilmstrip,
    goToPrevious,
    goToNext,
    goToIndex,
    // 图片加载相关
    getImageState,
    preloadImage,
  }
}
