import {
  computed,
  inject,
  onScopeDispose,
  provide,
  shallowReactive,
  shallowRef,
  watch,
  type InjectionKey,
  type ShallowRef,
} from 'vue'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'
import { useGalleryData } from './useGalleryData'

// 每次选择创建独立对象；即使快速切回同一资产，旧异步结果也不能接管新画面。
export interface LightboxImageRequest {
  asset: Asset
  url: string
  thumbnailUrl: string
  preview: 'loading' | 'thumbnail' | 'error'
  thumbnail: HTMLImageElement | null
  original: HTMLImageElement | null
  originalError: boolean
}

interface LightboxImageLoading {
  currentRequest: Readonly<ShallowRef<LightboxImageRequest | null>>
  getReadyOriginal: (asset: Asset) => HTMLImageElement | undefined
  notifyPreviewPresented: (assetId: number, request?: LightboxImageRequest) => void
}

interface PendingImage {
  image: HTMLImageElement
  ready: Promise<HTMLImageElement>
}

const imageLoadingKey: InjectionKey<LightboxImageLoading> = Symbol('lightboxImageLoading')
const PRELOAD_RANGE = 1

// 会话内唯一的图片加载所有者：预览准备 → 画面呈现 → 当前原图 → 可选邻图预热。
export function provideLightboxImageLoading(): LightboxImageLoading {
  const store = useGalleryStore()
  const galleryData = useGalleryData()
  const currentRequest = shallowRef<LightboxImageRequest | null>(null)
  const readyImages = shallowReactive(new Map<string, HTMLImageElement>())
  const pendingImages = new Map<string, PendingImage>()
  const currentAsset = computed(() => {
    const index = store.selection.activeIndex
    return index === undefined ? null : store.getAssetAt(index)
  })
  let pendingThumbnail: HTMLImageElement | null = null
  let previewPresented = false
  let rangeToken = 0
  let disposed = false

  const isActive = () => !disposed && store.lightbox.isOpen && !store.lightbox.isClosing
  const isCurrent = (request: LightboxImageRequest) =>
    isActive() && currentRequest.value === request

  function cancelThumbnail() {
    pendingThumbnail?.removeAttribute('src')
    pendingThumbnail = null
  }

  function loadOriginal(url: string): Promise<HTMLImageElement> {
    const cached = readyImages.get(url)
    if (cached) return Promise.resolve(cached)
    const pending = pendingImages.get(url)
    if (pending) return pending.ready

    const image = new Image()
    image.src = url
    const task: PendingImage = {
      image,
      ready: image
        .decode()
        .then(() => {
          if (pendingImages.get(url) === task) {
            // 保留解码结果供实际显示节点和相邻滑动预览复用。
            readyImages.set(url, image)
          }
          return image
        })
        .finally(() => {
          if (pendingImages.get(url) === task) pendingImages.delete(url)
        }),
    }
    pendingImages.set(url, task)
    return task.ready
  }

  function retainImages(urls: Set<string>, pendingUrls: Set<string>) {
    for (const url of readyImages.keys()) {
      if (!urls.has(url)) readyImages.delete(url)
    }
    for (const [url, task] of pendingImages) {
      if (!pendingUrls.has(url)) {
        pendingImages.delete(url)
        // 尽量停止无关请求；浏览器已经开始的解码不保证能立即中断。
        task.image.removeAttribute('src')
      }
    }
  }

  async function preloadAdjacent(index: number, token: number) {
    let asset = store.getAssetAt(index)
    if (!asset) {
      await galleryData.ensureIndexesLoaded([index])
      if (token !== rangeToken) return
      asset = store.getAssetAt(index)
    }
    if (token !== rangeToken || !asset || asset.type === 'video') return
    await loadOriginal(galleryApi.getAssetUrl(asset))
  }

  function refreshNeighbors() {
    const token = ++rangeToken
    const index = store.selection.activeIndex
    const asset = currentAsset.value
    if (!isActive() || index === undefined || !asset) {
      cancelThumbnail()
      retainImages(new Set(), new Set())
      return
    }

    const currentUrl = asset.type === 'video' ? '' : galleryApi.getAssetUrl(asset)
    const urls = new Set(currentUrl ? [currentUrl] : [])
    const indexes: number[] = []
    if (store.view.preloadOriginalImagesForLightbox) {
      for (let offset = 1; offset <= PRELOAD_RANGE; offset += 1) {
        for (const neighbor of [index + offset, index - offset]) {
          if (neighbor < 0 || neighbor >= store.totalCount) continue
          indexes.push(neighbor)
          const item = store.getAssetAt(neighbor)
          if (item && item.type !== 'video') urls.add(galleryApi.getAssetUrl(item))
        }
      }
    }

    const currentOriginalReady = Boolean(currentRequest.value?.original)
    const canPreload = previewPresented && (currentOriginalReady || asset.type === 'video')
    // 新预览接管前，连目标自身尚在预热的原图也停止，优先把资源让给缩略图。
    retainImages(
      urls,
      canPreload ? urls : new Set(previewPresented && currentUrl ? [currentUrl] : [])
    )
    if (canPreload) {
      for (const neighbor of indexes) {
        void preloadAdjacent(neighbor, token).catch((error: unknown) => {
          if (token === rangeToken) console.warn('Failed to preload lightbox image:', error)
        })
      }
    }
  }

  async function prepareThumbnail(request: LightboxImageRequest) {
    const image = new Image()
    pendingThumbnail = image
    image.src = request.thumbnailUrl
    try {
      await image.decode()
      if (!isCurrent(request)) return
      request.thumbnail = image
      request.preview = 'thumbnail'
    } catch {
      if (isCurrent(request)) request.preview = 'error'
    } finally {
      if (pendingThumbnail === image) pendingThumbnail = null
    }
  }

  async function loadCurrentOriginal(request: LightboxImageRequest) {
    try {
      const image = await loadOriginal(request.url)
      if (isCurrent(request)) request.original = image
      if (isCurrent(request)) refreshNeighbors()
    } catch {
      if (isCurrent(request)) request.originalError = true
    }
  }

  // GalleryLightbox 完成入场和可见帧交接后调用；当前原图任务只从这里启动。
  function notifyPreviewPresented(assetId: number, request?: LightboxImageRequest) {
    if (!isActive() || currentAsset.value?.id !== assetId) return
    if (request ? !isCurrent(request) : currentRequest.value !== null) return
    if (previewPresented) return
    previewPresented = true
    if (request && !request.original) void loadCurrentOriginal(request)
    refreshNeighbors()
  }

  watch(
    [
      () => store.lightbox.isOpen,
      () => store.selection.activeIndex,
      () => currentAsset.value?.id,
      () => currentAsset.value?.hash,
      () => (currentAsset.value ? galleryApi.getAssetUrl(currentAsset.value) : ''),
    ],
    () => {
      cancelThumbnail()
      previewPresented = false
      currentRequest.value = null
      refreshNeighbors()

      const asset = currentAsset.value
      if (!isActive() || !asset || asset.type === 'video') return
      const url = galleryApi.getAssetUrl(asset)
      const cached = readyImages.get(url) ?? null
      const request = shallowReactive<LightboxImageRequest>({
        asset,
        url,
        thumbnailUrl: galleryApi.getThumbnailUrl(asset.hash),
        preview: 'loading',
        thumbnail: null,
        original: cached,
        originalError: false,
      })
      currentRequest.value = request
      // 缓存只代表离屏原图就绪；新显示节点解码期间仍需要缩略图托底。
      void prepareThumbnail(request)
    },
    { immediate: true, flush: 'sync' }
  )

  watch(
    () => [
      store.lightbox.isClosing,
      store.view.preloadOriginalImagesForLightbox,
      store.queryVersion,
      store.paginatedAssetsVersion,
      store.totalCount,
      store.perPage,
    ],
    refreshNeighbors
  )

  const loading: LightboxImageLoading = {
    currentRequest,
    getReadyOriginal: (asset) => readyImages.get(galleryApi.getAssetUrl(asset)),
    notifyPreviewPresented,
  }
  provide(imageLoadingKey, loading)
  onScopeDispose(() => {
    disposed = true
    rangeToken += 1
    cancelThumbnail()
    retainImages(new Set(), new Set())
  })
  return loading
}

export function useLightboxImageLoading(): LightboxImageLoading {
  const loading = inject(imageLoadingKey)
  if (!loading) throw new Error('Lightbox image loading must be provided by GalleryLightbox')
  return loading
}
