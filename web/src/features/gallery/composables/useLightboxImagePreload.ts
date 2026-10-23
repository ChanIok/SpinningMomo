import {
  inject,
  nextTick,
  onScopeDispose,
  provide,
  shallowReactive,
  watch,
  type InjectionKey,
} from 'vue'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'
import { useGalleryData } from './useGalleryData'

interface LightboxImagePreload {
  getReadyImage: (asset: Asset) => HTMLImageElement | undefined
  notifyCurrentMediaReady: (assetId: number, url: string) => void
}

const lightboxImagePreloadKey: InjectionKey<LightboxImagePreload> = Symbol('lightboxImagePreload')
const PRELOAD_RANGE = 1

// 每个暗房会话只创建一个缓存，当前图片和滑动预览共用已经解码的原图。
export function provideLightboxImagePreload() {
  const store = useGalleryStore()
  const galleryData = useGalleryData()
  const readyImages = shallowReactive(new Map<string, HTMLImageElement>())
  const pendingImages = new Map<string, HTMLImageElement>()
  let currentMediaKey = ''
  let displayedMediaKey = ''
  let rangeToken = 0
  let displayToken = 0
  let displayFrame: number | null = null
  let disposed = false

  function cancelDisplayNotification() {
    displayToken += 1
    if (displayFrame !== null) {
      cancelAnimationFrame(displayFrame)
      displayFrame = null
    }
  }

  function preloadImage(url: string) {
    if (readyImages.has(url) || pendingImages.has(url)) {
      return
    }

    const image = new Image()
    pendingImages.set(url, image)
    image.src = url
    void image
      .decode()
      .then(() => {
        if (pendingImages.get(url) === image) {
          // 保留图片引用和解码结果，不能只记录曾经触发过 load 的资产 id。
          readyImages.set(url, image)
        }
      })
      .catch((error: unknown) => {
        if (pendingImages.get(url) === image) {
          console.warn('Failed to preload lightbox image:', url, error)
        }
      })
      .finally(() => {
        if (pendingImages.get(url) === image) {
          pendingImages.delete(url)
        }
      })
  }

  function retainImages(urls: Set<string>, pendingUrls = urls) {
    for (const url of readyImages.keys()) {
      if (!urls.has(url)) {
        readyImages.delete(url)
      }
    }
    for (const [url, image] of pendingImages) {
      if (!pendingUrls.has(url)) {
        pendingImages.delete(url)
        // 让旧结果失效，并尽量停止无关请求；已进入内核的解码不保证能立即中断。
        image.removeAttribute('src')
      }
    }
  }

  async function preloadAdjacentImage(index: number, token: number) {
    if (token !== rangeToken) {
      return
    }

    let asset = store.getAssetAt(index)
    if (!asset) {
      // 当前媒体显示后才查询相邻分页，且不阻塞另一侧的原图准备。
      await galleryData.ensureIndexesLoaded([index])
      if (token !== rangeToken) {
        return
      }
      asset = store.getAssetAt(index)
    }

    if (asset?.type === 'photo' || asset?.type === 'live_photo') {
      const url = galleryApi.getAssetUrl(asset)
      if (url) {
        preloadImage(url)
      }
    }
  }

  function refreshRange() {
    const token = ++rangeToken
    const activeIndex = store.selection.activeIndex
    const asset = activeIndex === undefined ? null : store.getAssetAt(activeIndex)
    const currentUrl = asset ? galleryApi.getAssetUrl(asset) : ''
    const key = store.lightbox.isOpen && asset ? `${asset.id}:${currentUrl}` : ''
    if (key !== currentMediaKey) {
      currentMediaKey = key
      displayedMediaKey = ''
      cancelDisplayNotification()
    }

    if (
      !store.lightbox.isOpen ||
      !store.view.preloadOriginalImagesForLightbox ||
      activeIndex === undefined ||
      !asset
    ) {
      retainImages(new Set())
      return
    }

    const indexes = [activeIndex]
    for (let offset = 1; offset <= PRELOAD_RANGE; offset += 1) {
      indexes.push(activeIndex + offset, activeIndex - offset)
    }
    const validIndexes = indexes.filter((index) => index >= 0 && index < store.totalCount)
    const urls = new Set<string>()
    for (const index of validIndexes) {
      const item = store.getAssetAt(index)
      // 实况仅预热静态原图；视频继续由播放器按需加载。
      if (item?.type === 'photo' || item?.type === 'live_photo') {
        const url = galleryApi.getAssetUrl(item)
        if (url) {
          urls.add(url)
        }
      }
    }

    const currentDisplayed = displayedMediaKey === key
    // 已解码的相邻图继续保留；当前图未显示时，只保留新目标自身的在途任务。
    retainImages(urls, currentDisplayed ? urls : new Set([currentUrl]))
    if ((asset.type === 'photo' || asset.type === 'live_photo') && currentUrl) {
      preloadImage(currentUrl)
    }
    if (currentDisplayed) {
      // 两侧各自准备，不因另一张的加载、解码或分页查询而等待。
      for (const index of validIndexes) {
        if (index === activeIndex) continue
        void preloadAdjacentImage(index, token).catch((error) => {
          console.warn('Failed to preload adjacent lightbox image:', error)
        })
      }
    }
  }

  function notifyCurrentMediaReady(assetId: number, url: string) {
    const activeIndex = store.selection.activeIndex
    const asset = activeIndex === undefined ? null : store.getAssetAt(activeIndex)
    if (
      disposed ||
      !store.lightbox.isOpen ||
      !asset ||
      asset.id !== assetId ||
      galleryApi.getAssetUrl(asset) !== url ||
      displayedMediaKey === currentMediaKey
    ) {
      return
    }

    cancelDisplayNotification()
    const token = displayToken
    const isCurrent = () => !disposed && token === displayToken
    // 原图已解码（视频已有首帧）后，先提交可见状态并留出一个绘制帧，再启动后台任务。
    void nextTick().then(() => {
      if (!isCurrent()) return
      displayFrame = requestAnimationFrame(() => {
        if (!isCurrent()) return
        displayFrame = requestAnimationFrame(() => {
          displayFrame = null
          if (!isCurrent()) return
          displayedMediaKey = currentMediaKey
          refreshRange()
        })
      })
    })
  }

  watch(
    () => ({
      isOpen: store.lightbox.isOpen,
      enabled: store.view.preloadOriginalImagesForLightbox,
      activeIndex: store.selection.activeIndex,
      queryVersion: store.queryVersion,
      assetsVersion: store.paginatedAssetsVersion,
      totalCount: store.totalCount,
      perPage: store.perPage,
    }),
    refreshRange,
    { immediate: true }
  )

  provide(lightboxImagePreloadKey, {
    getReadyImage: (asset) =>
      store.view.preloadOriginalImagesForLightbox
        ? readyImages.get(galleryApi.getAssetUrl(asset))
        : undefined,
    notifyCurrentMediaReady,
  })
  onScopeDispose(() => {
    disposed = true
    rangeToken += 1
    cancelDisplayNotification()
    retainImages(new Set())
  })
}

export function useLightboxImagePreload(): LightboxImagePreload {
  const preload = inject(lightboxImagePreloadKey)
  if (!preload) {
    throw new Error('Lightbox image preload must be provided by GalleryLightbox')
  }
  return preload
}
