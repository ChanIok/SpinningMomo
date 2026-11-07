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
import { LIGHTBOX_TILE_THRESHOLD_PIXELS } from '../constants'
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
  ensureOriginal: (request: LightboxImageRequest) => Promise<HTMLImageElement | null>
  notifyPreviewPresented: (assetId: number, request?: LightboxImageRequest) => void
  waitForPreviewPresented: (request: LightboxImageRequest) => Promise<boolean>
  notifyOriginalPresented: (assetId: number, request: LightboxImageRequest) => void
}

interface PendingImage {
  image: HTMLImageElement
  ready: Promise<HTMLImageElement>
}

const imageLoadingKey: InjectionKey<LightboxImageLoading> = Symbol('lightboxImageLoading')
const PRELOAD_RANGE = 1

/** 判断暗房原图是否需要走分块渲染路径；普通原图走 <img>，超过阈值交给分块画布。 */
export function shouldUseLightboxTiling(width: number, height: number): boolean {
  return width > 0 && height > 0 && width * height > LIGHTBOX_TILE_THRESHOLD_PIXELS
}

// 等待一个 img 真正完成加载和解码，避免只凭离屏请求状态切换画面。
export async function waitForLightboxImageReady(image: HTMLImageElement | null): Promise<boolean> {
  if (!image) return false

  const source = image.currentSrc || image.src
  if (!image.complete) {
    await new Promise<void>((resolve) => {
      let settled = false
      const settle = () => {
        if (settled) return
        settled = true
        image.removeEventListener('load', settle)
        image.removeEventListener('error', settle)
        resolve()
      }

      image.addEventListener('load', settle)
      image.addEventListener('error', settle)
      if (image.complete) settle()
    })
  }

  if (image.naturalWidth <= 0 || (image.currentSrc || image.src) !== source) return false

  try {
    await image.decode()
  } catch {
    // load 已经成功且有像素时，decode 的拒绝不应阻止当前图片显示。
  }

  return image.naturalWidth > 0 && (image.currentSrc || image.src) === source
}

// 等待两帧，让新图层完成一次可见绘制后再隐藏旧图层。
export function waitForLightboxPaint(): Promise<void> {
  return new Promise((resolve) => {
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()))
  })
}

// 建立会话内唯一的图片加载所有者，串联预览、当前原图和邻图预热。
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
  let previewPresentedRequest: LightboxImageRequest | null = null
  let previewWaiter: {
    request: LightboxImageRequest
    resolve: (value: boolean) => void
  } | null = null
  let originalPresentedRequest: LightboxImageRequest | null = null
  let rangeToken = 0
  let disposed = false

  // 只允许当前打开的暗房会话继续发起加载和预热。
  const isActive = () => !disposed && store.lightbox.isOpen && !store.lightbox.isClosing
  const isCurrent = (request: LightboxImageRequest) =>
    isActive() && currentRequest.value === request

  // 结算等待预览呈现的 Promise，避免切图后留下悬挂任务。
  function settlePreviewWaiters(request: LightboxImageRequest, value: boolean) {
    if (previewWaiter?.request !== request) return
    const resolve = previewWaiter.resolve
    previewWaiter = null
    resolve(value)
  }

  // 等待 GalleryLightbox 把当前缩略图真正交给用户可见层。
  function waitForPreviewPresented(request: LightboxImageRequest): Promise<boolean> {
    if (previewPresentedRequest === request) return Promise.resolve(true)
    if (!isCurrent(request)) return Promise.resolve(false)
    return new Promise((resolve) => {
      if (previewWaiter && previewWaiter.request !== request) {
        previewWaiter.resolve(false)
      }
      previewWaiter = { request, resolve }
    })
  }

  // 停止上一张缩略图的离屏请求，让资源优先给当前目标。
  function cancelThumbnail() {
    pendingThumbnail?.removeAttribute('src')
    pendingThumbnail = null
  }

  // 按 URL 复用已解码原图，并合并同一 URL 的并发请求。
  function loadOriginal(url: string): Promise<HTMLImageElement> {
    const cached = readyImages.get(url)
    if (cached) {
      // 已完成解码的原图直接复用，避免再次触发网络和解码。
      return Promise.resolve(cached)
    }
    const pending = pendingImages.get(url)
    if (pending) {
      // 同 URL 已在加载时共享同一个 Promise，避免重复请求。
      return pending.ready
    }

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

  // 删除超出当前保留范围的普通原图和未完成请求。
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

  // 预热一个相邻普通图片；大图交给 tiled renderer 单独处理。
  async function preloadAdjacent(index: number, token: number) {
    let asset = store.getAssetAt(index)
    if (!asset) {
      await galleryData.ensureIndexesLoaded([index])
      if (token !== rangeToken) return
      asset = store.getAssetAt(index)
    }
    if (token !== rangeToken || !asset || asset.type === 'video') return

    const url = galleryApi.getAssetUrl(asset)
    if (shouldUseLightboxTiling(asset.width ?? 0, asset.height ?? 0)) return

    await loadOriginal(url)
  }

  // 根据当前选择刷新普通原图缓存，并按会话状态启动相邻预热。
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
          // 保留范围内已有的原图；是否启动大图解码由 preloadAdjacent 单独决定。
          if (item && item.type !== 'video') {
            urls.add(galleryApi.getAssetUrl(item))
          }
        }
      }
    }

    const currentOriginalReady = Boolean(currentRequest.value?.original)
    const currentVisualReady = originalPresentedRequest === currentRequest.value
    const currentUsesTiling = shouldUseLightboxTiling(asset.width ?? 0, asset.height ?? 0)
    const canPreload =
      previewPresented &&
      (asset.type === 'video' ||
        (currentVisualReady && (currentOriginalReady || currentUsesTiling)))
    // 新预览接管前，连目标自身尚在预热的原图也停止，优先把资源让给缩略图。
    retainImages(
      urls,
      canPreload ? urls : new Set(previewPresented && currentUrl ? [currentUrl] : [])
    )
    if (canPreload) {
      // 只有当前画面已经接管后，才把网络和解码资源交给相邻图片。
      for (const neighbor of indexes) {
        void preloadAdjacent(neighbor, token).catch((error: unknown) => {
          if (token === rangeToken) console.warn('Failed to preload lightbox image:', error)
        })
      }
    }
  }

  // 解码当前缩略图，并把结果写回当前请求对象。
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
      // 缩略图失败仍保留请求，让上层显示错误或尝试根映射恢复。
      if (isCurrent(request)) request.preview = 'error'
    } finally {
      if (pendingThumbnail === image) pendingThumbnail = null
    }
  }

  // 在预览接管后加载当前普通原图，并把失败状态绑定到当前请求。
  async function loadCurrentOriginal(request: LightboxImageRequest) {
    if (!isCurrent(request)) return null
    try {
      const image = await loadOriginal(request.url)
      if (!isCurrent(request)) return null
      request.original = image
      request.originalError = false
      return image
    } catch (error: unknown) {
      if (isCurrent(request)) {
        // 只记录当前请求的失败，旧请求的异常不能污染新画面。
        request.originalError = true
        console.warn('Failed to load lightbox original:', error)
      }
      return null
    }
  }

  // GalleryLightbox 完成入场和可见帧交接后，标记预览并启动后续加载。
  function notifyPreviewPresented(assetId: number, request?: LightboxImageRequest) {
    if (!isActive() || currentAsset.value?.id !== assetId) return
    if (request ? !isCurrent(request) : currentRequest.value !== null) return
    if (previewPresented) return
    previewPresented = true
    // 唤醒等待当前预览的原图任务。
    previewPresentedRequest = request ?? currentRequest.value
    if (previewPresentedRequest) settlePreviewWaiters(previewPresentedRequest, true)
    // 普通图片从这里开始加载；大图由 LightboxImage 调用 tiled renderer。
    if (
      request &&
      !request.original &&
      !shouldUseLightboxTiling(request.asset.width ?? 0, request.asset.height ?? 0)
    ) {
      void loadCurrentOriginal(request)
    }
    refreshNeighbors()
  }

  // 当前图完成实际画面接管后，才允许启动邻图原图预热。
  function notifyOriginalPresented(assetId: number, request: LightboxImageRequest) {
    if (
      !isActive() ||
      currentAsset.value?.id !== assetId ||
      !isCurrent(request) ||
      (!request.original &&
        !shouldUseLightboxTiling(request.asset.width ?? 0, request.asset.height ?? 0))
    ) {
      return
    }

    originalPresentedRequest = request
    refreshNeighbors()
  }

  // 选择变化或关闭暗房时重建请求对象，淘汰旧异步结果。
  watch(
    [
      () => store.lightbox.isOpen,
      () => store.selection.activeIndex,
      () => currentAsset.value?.id,
      () => currentAsset.value?.hash,
      () => (currentAsset.value ? galleryApi.getAssetUrl(currentAsset.value) : ''),
    ],
    () => {
      // 选择、URL 或暗房状态变化后，旧请求不再拥有画面接管权。
      cancelThumbnail()
      if (currentRequest.value) settlePreviewWaiters(currentRequest.value, false)
      previewPresented = false
      previewPresentedRequest = null
      originalPresentedRequest = null
      currentRequest.value = null
      refreshNeighbors()

      const asset = currentAsset.value
      if (!isActive() || !asset || asset.type === 'video') return
      // 为新资产创建独立请求，避免快速切回同一资产时复用旧异步状态。
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
    ensureOriginal: loadCurrentOriginal,
    notifyPreviewPresented,
    waitForPreviewPresented,
    notifyOriginalPresented,
  }
  provide(imageLoadingKey, loading)
  onScopeDispose(() => {
    disposed = true
    rangeToken += 1
    cancelThumbnail()
    retainImages(new Set(), new Set())
    if (previewWaiter) {
      previewWaiter.resolve(false)
      previewWaiter = null
    }
  })
  return loading
}

// 读取 GalleryLightbox 提供的单例图片加载会话。
export function useLightboxImageLoading(): LightboxImageLoading {
  const loading = inject(imageLoadingKey)
  if (!loading) throw new Error('Lightbox image loading must be provided by GalleryLightbox')
  return loading
}
