<script setup lang="ts">
import { computed, nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import { useEventListener, useThrottleFn } from '@vueuse/core'
import { useGalleryAssetActions, useGalleryLightbox, useGallerySelection } from '../../composables'
import type { LightboxVerticalGestureAction } from '../../composables/useLightboxSwipeNavigation'
import {
  isGalleryLightboxOverlay,
  useGalleryOverlayHistory,
} from '../../composables/useGalleryOverlayHistory'
import { useGalleryStore } from '../../store'
import {
  computeLightboxHeroRect,
  consumeHero,
  endHeroAnimation,
  LIGHTBOX_VIEWPORT_PADDING,
  rectToFixedStyle,
} from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import GalleryAssetContextMenuContent from '../menus/GalleryAssetContextMenuContent.vue'
import GalleryDetails from '../shell/GalleryDetails.vue'
import LightboxFilmstrip from './LightboxFilmstrip.vue'
import LightboxNavigationButtons from './LightboxNavigationButtons.vue'
import LightboxPager from './LightboxPager.vue'
import LightboxToolbar from './LightboxToolbar.vue'
import GalleryMobileActionBar from '../mobile/GalleryMobileActionBar.vue'
import { Button } from '@/components/ui/button'
import { ContextMenu, ContextMenuContent, ContextMenuTrigger } from '@/components/ui/context-menu'
import { MobileDrawer } from '@/components/ui/mobile-drawer'
import { isLocalAccess } from '@/core/access'
import { useI18n } from '@/composables/useI18n'
import { X } from '@lucide/vue'
import {
  isGalleryTouchContextMenu,
  isGalleryTouchInput,
  normalizeGalleryInputType,
} from '../../input'
import type { GalleryInputType } from '../../input'
import type { Asset } from '../../types'

/** 与反向 hero、surface 淡出时长（约 220ms）对齐，并留出双 rAF 余量 */
const CLOSE_AFTER_REVERSE_HERO_MS = 260
/** 无飞回动画时，与工具栏/内容区 leave ~180ms 对齐 */
const CLOSE_AFTER_NO_HERO_MS = 180
/** 静态图单击先等待双击窗口，避免双击时 chrome 先闪一次。 */
const TOUCH_SINGLE_TAP_DELAY_MS = 300
/** 抽屉进入动画完成后再挂载详情树，避免重组件参与首帧位移动画。 */
const DETAILS_CONTENT_DELAY_MS = 260
/** 下拉未提交时让媒体表面回到原位的动画时长。 */
const VERTICAL_GESTURE_SNAPBACK_MS = 220

type LightboxPagerExposed = {
  showFitMode: () => Promise<void>
  showActualSize: () => Promise<void>
  zoomIn: () => Promise<void>
  zoomOut: () => Promise<void>
}

type GalleryContentRef = {
  getCardRect: (index: number) => DOMRect | null
} | null

type HeroViewport = {
  rect: DOMRect
  padding: number
}

type VerticalGesturePhase = 'idle' | 'dragging' | 'settling'

const props = defineProps<{
  galleryContentRef: GalleryContentRef
}>()

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const gallerySelection = useGallerySelection()
const overlayHistory = useGalleryOverlayHistory()
const assetActions = useGalleryAssetActions()
const lightboxPagerRef = ref<LightboxPagerExposed | null>(null)
const lightboxRootRef = ref<HTMLElement | null>(null)
const mediaViewportRef = ref<HTMLElement | null>(null)
// 纵向拖拽只直接更新媒体表面，避免每个 pointermove 触发整个 GalleryLightbox 重渲染。
const mediaGestureSurfaceRef = ref<HTMLElement | null>(null)
// 详情抽屉是否打开由历史快照决定，浏览器返回和界面按钮都能驱动同一个状态。
const mobileDetailsOpen = computed(
  () => overlayHistory.snapshot.value.overlay === 'lightbox-details'
)
const detailsContentReady = ref(false)
let detailsContentTimer: number | null = null
let verticalGestureOffset = 0
let verticalGesturePhase: VerticalGesturePhase = 'idle'
let verticalGestureRafId: number | null = null
let pendingVerticalGestureOffset: number | null = null
let verticalGestureResetTimer: number | null = null
// 下拉关闭会先消费暗房历史，再异步启动退场动画；这里暂存松手时的视觉偏移。
let pendingExitGestureOffset: number | null = null
// 界面层显隐与沉浸模式分离；共享状态让全局 App Header 与暗房控件同步。
const isLightboxChromeVisible = computed(() => store.lightbox.chromeVisible)
let pendingTouchTapTimer: number | null = null
// inputType 记录打开来源；这里单独记录会话内最近一次指针模态，支持混合触控设备切换。
const activeInputType = ref<GalleryInputType>('mouse')
const preloadingAssetIds = new Set<number>()
const preloadedAssetIds = new Set<number>()
// 暗房工具栏紧凑模式与窗口级紧凑状态保持一致。
const isToolbarCompressed = computed(() => store.isCompactWindow)
const { t } = useI18n()

watch(isToolbarCompressed, (compressed) => {
  if (!compressed) {
    // 详情抽屉只属于紧凑工具栏，窗口变宽时同步消费其历史层。
    closeMobileDetails()
  }
})

// 预加载只由唯一的暗房实例协调，避免 GalleryLightbox/Pager/Image 各自注册重复 watcher。
watch(
  () => store.selection.activeIndex,
  (newIndex, oldIndex) => {
    if (!store.lightbox.isOpen || newIndex === undefined) {
      return
    }

    if (newIndex !== oldIndex) {
      clearPendingTouchTap()
      store.resetLightboxView()
    }

    void preloadRange(newIndex).catch((error) => {
      console.warn('Failed to preload lightbox range:', error)
    })
  },
  { immediate: true }
)

const isImmersive = computed(() => store.lightbox.isImmersive)
const isClosing = computed(() => store.lightbox.isClosing)
// 普通桌面模式给工具栏和底片栏留出真实布局空间；紧凑/沉浸模式继续覆盖媒体。
const isReservedDesktopLayout = computed(() => !store.isCompactWindow && !isImmersive.value)
const isTouchInput = computed(() => isGalleryTouchInput(activeInputType.value))
// 只有窄屏触摸采用相册式“轻点切换界面”的沉浸交互；宽屏触摸保留工作区控件。
const canToggleChromeByTap = computed(() => store.isCompactWindow && isTouchInput.value)
// 纵向相册手势只在紧凑触摸暗房且详情层未覆盖媒体时启用。
const canUseVerticalGesture = computed(
  () =>
    store.isCompactWindow &&
    isTouchInput.value &&
    !isClosing.value &&
    !mobileDetailsOpen.value &&
    store.lightbox.isOpen
)
// 底片栏属于窗口级布局；它必须跟随整个应用窗口，而不是暗房中间内容区。
const showFilmstrip = computed(() => !store.isCompactWindow && store.lightbox.showFilmstrip)
const fitMode = computed(() => store.lightbox.fitMode)
const currentAsset = computed(() => {
  const currentIndex = store.selection.activeIndex
  if (currentIndex === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIndex, currentIndex)[0]
})
const canGoToPrevious = computed(() => (store.selection.activeIndex ?? 0) > 0)
const canGoToNext = computed(() => (store.selection.activeIndex ?? 0) < store.totalCount - 1)
// 缩放、适屏、1:1 都是静态图查看语义；视频在灯箱里保持原生播放器行为。
const isZoomableAsset = computed(() => currentAsset.value?.type !== 'video')
const lightboxRootClass = computed(() => {
  const immersive = isImmersive.value
  const closing = store.lightbox.isClosing
  let cls = immersive
    ? 'surface-bottom fixed inset-0 z-[100] flex overflow-hidden shadow-2xl px-[1px]'
    : 'absolute inset-0 z-10 flex h-full w-full overflow-hidden px-[1px]'
  if (immersive && closing) {
    cls += ' pointer-events-none opacity-0 transition-opacity duration-[280ms] ease-out'
  }
  return cls
})

function clearDetailsContentTimer() {
  if (detailsContentTimer !== null) {
    window.clearTimeout(detailsContentTimer)
    detailsContentTimer = null
  }
}

function scheduleDetailsContentMount() {
  clearDetailsContentTimer()
  detailsContentReady.value = false

  if (!mobileDetailsOpen.value || isClosing.value) {
    return
  }

  detailsContentTimer = window.setTimeout(() => {
    detailsContentTimer = null
    if (mobileDetailsOpen.value && !isClosing.value) {
      detailsContentReady.value = true
    }
  }, DETAILS_CONTENT_DELAY_MS)
}

watch(
  mobileDetailsOpen,
  (open) => {
    if (open) {
      scheduleDetailsContentMount()
    } else {
      clearDetailsContentTimer()
      detailsContentReady.value = false
    }
  },
  { immediate: true }
)

watch(
  () => store.lightbox.isOpen,
  (isOpen) => {
    clearPendingTouchTap()
    if (!isOpen) {
      resetVerticalGestureSurface()
      return
    }

    activeInputType.value = store.lightbox.inputType
  },
  { immediate: true }
)

watch(canToggleChromeByTap, (canToggle) => {
  if (!canToggle && !store.lightbox.chromeVisible) {
    // 离开窄屏触摸环境后恢复工作区控件，避免 Header 被永久留在隐藏状态。
    store.setLightboxChromeVisible(true)
  }
})

watch(
  () => overlayHistory.snapshot.value.overlay,
  (overlay, previousOverlay) => {
    // 路由先完成历史回退，动画只在真正离开暗房层时启动，避免详情关闭误触发退场。
    if (
      isGalleryLightboxOverlay(previousOverlay) &&
      !isGalleryLightboxOverlay(overlay) &&
      store.lightbox.isOpen
    ) {
      animateClose()
    }
  }
)

function handleLightboxPointerDown(event: PointerEvent) {
  activeInputType.value = normalizeGalleryInputType(event.pointerType)
}

function isPreloadableImageAsset(asset: Asset | null): boolean {
  // 视频交给 <video> 自己按需拉取分片；这里的图片预热只服务 still image 的秒开体验。
  return asset?.type === 'photo' || asset?.type === 'live_photo'
}

/** 使用 Image 写入浏览器缓存；预加载状态只属于当前暗房实例。 */
async function preloadImage(asset: Asset): Promise<void> {
  if (
    !isPreloadableImageAsset(asset) ||
    preloadedAssetIds.has(asset.id) ||
    preloadingAssetIds.has(asset.id)
  ) {
    return
  }

  preloadingAssetIds.add(asset.id)
  const url = galleryApi.getAssetUrl(asset)

  return new Promise((resolve, reject) => {
    const img = new Image()
    img.onload = () => {
      preloadingAssetIds.delete(asset.id)
      preloadedAssetIds.add(asset.id)
      resolve()
    }
    img.onerror = () => {
      preloadingAssetIds.delete(asset.id)
      reject(new Error(`Failed to load image: ${asset.id}`))
    }
    img.src = url
  })
}

async function preloadRange(currentIndex: number) {
  const PRELOAD_RANGE = 2
  const start = Math.max(0, currentIndex - PRELOAD_RANGE)
  const end = Math.min(store.totalCount - 1, currentIndex + PRELOAD_RANGE)
  const currentAsset = store.getAssetsInRange(currentIndex, currentIndex)[0]
  if (!currentAsset) {
    return
  }

  if (isPreloadableImageAsset(currentAsset)) {
    try {
      await preloadImage(currentAsset)
    } catch (error) {
      console.warn(
        `Failed to preload current image [index=${currentIndex}, id=${currentAsset.id}]`,
        error
      )
    }
  }

  const preloadPromises: Promise<void>[] = []
  for (let offset = 1; offset <= PRELOAD_RANGE; offset += 1) {
    const indexes = [currentIndex + offset, currentIndex - offset]
    for (const index of indexes) {
      if (index < start || index > end) {
        continue
      }

      const asset = store.getAssetsInRange(index, index)[0]
      if (asset) {
        preloadPromises.push(
          preloadImage(asset).catch((error) => {
            console.warn(`Failed to preload image [index=${index}, id=${asset.id}]`, error)
          })
        )
      }
    }
  }

  await Promise.allSettled(preloadPromises)
}

const throttledPrevious = useThrottleFn(() => {
  if (store.lightbox.isOpen) {
    lightbox.goToPrevious()
  }
}, 200)

const throttledNext = useThrottleFn(() => {
  if (store.lightbox.isOpen) {
    lightbox.goToNext()
  }
}, 200)

const throttledZoomIn = useThrottleFn(() => {
  if (store.lightbox.isOpen) {
    void lightboxPagerRef.value?.zoomIn()
  }
}, 60)

const throttledZoomOut = useThrottleFn(() => {
  if (store.lightbox.isOpen) {
    void lightboxPagerRef.value?.zoomOut()
  }
}, 60)

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

function enterImmersive() {
  if (isImmersive.value) {
    return
  }
  lightbox.setImmersive(true)
}

function exitImmersive() {
  if (!isImmersive.value) {
    return
  }
  lightbox.setImmersive(false)
}

// 播放暗房退场动画；动画完成后再清理选择，避免反向 hero 动画失去来源卡片。
function animateClose() {
  clearPendingTouchTap()

  if (store.lightbox.isClosing) {
    pendingExitGestureOffset = null
    return
  }

  const exitGestureOffset = pendingExitGestureOffset
  pendingExitGestureOffset = null
  store.setLightboxClosing(true)

  let didReverseHero = false
  // gallery 在打开时用 opacity 隐藏但仍可布局，可直接同步读取 cardRect
  const activeIndex = store.selection.activeIndex
  if (activeIndex !== undefined) {
    const galleryContent = props.galleryContentRef
    if (galleryContent) {
      const cardRect = galleryContent.getCardRect(activeIndex)
      const asset = store.getAssetsInRange(activeIndex, activeIndex)[0]
      const heroViewport = getHeroViewport()
      if (cardRect && asset && heroViewport) {
        let fromRect = computeLightboxHeroRect(
          heroViewport.rect,
          asset.width ?? 1,
          asset.height ?? 1,
          heroViewport.padding
        )
        if (exitGestureOffset !== null) {
          fromRect = transformHeroRectForVerticalGesture(fromRect, exitGestureOffset)
        }
        startExitReverseHero(fromRect, cardRect, galleryApi.getAssetThumbnailUrl(asset))
        didReverseHero = true
      }
    }
  }

  const delay = didReverseHero ? CLOSE_AFTER_REVERSE_HERO_MS : CLOSE_AFTER_NO_HERO_MS
  // 暗房关闭后保留当前选择，方便用户回到图库继续操作或查看详情；没有选择时才清理临时焦点。
  const shouldClearBrowseFocus = store.selectedCount === 0
  window.setTimeout(() => {
    lightbox.closeLightbox()
    if (shouldClearBrowseFocus) {
      store.clearActiveAsset()
    }
  }, delay)
}

// Hero 的目标区域必须和图片实际可见的 viewport 一致，避免普通桌面模式落到上下 chrome 下方。
function getHeroViewport(): HeroViewport | null {
  const mediaViewport = mediaViewportRef.value
  const root = lightboxRootRef.value
  const element = isReservedDesktopLayout.value ? (mediaViewport ?? root) : root
  if (!element) {
    return null
  }

  return {
    rect: element.getBoundingClientRect(),
    padding: element === mediaViewport ? 0 : LIGHTBOX_VIEWPORT_PADDING,
  }
}

function getVerticalGestureViewportHeight(): number {
  return Math.max(
    mediaViewportRef.value?.clientHeight ??
      (typeof window !== 'undefined' ? window.innerHeight : 1),
    1
  )
}

function getVerticalGestureFadeProgress(offsetY: number): number {
  const positiveOffset = Math.max(offsetY, 0)
  return Math.min(positiveOffset / Math.max(getVerticalGestureViewportHeight() * 0.65, 240), 1)
}

function getVerticalGestureScale(offsetY: number): number {
  return 1 - getVerticalGestureFadeProgress(offsetY) * 0.04
}

function transformHeroRectForVerticalGesture(rect: DOMRect, offsetY: number): DOMRect {
  const positiveOffset = Math.max(offsetY, 0)
  const scale = getVerticalGestureScale(positiveOffset)
  const width = rect.width * scale
  const height = rect.height * scale

  // mediaGestureSurface 的 transform-origin 是中心点；Hero 起点同步应用同一组缩放和下移。
  return new DOMRect(
    rect.left + (rect.width - width) / 2,
    rect.top + positiveOffset + (rect.height - height) / 2,
    width,
    height
  )
}

function clearVerticalGestureFrame() {
  if (verticalGestureRafId !== null) {
    cancelAnimationFrame(verticalGestureRafId)
    verticalGestureRafId = null
  }
  pendingVerticalGestureOffset = null
}

function applyVerticalGestureSurfaceStyle(offsetY: number, phase: VerticalGesturePhase) {
  const surface = mediaGestureSurfaceRef.value
  if (!surface) {
    return
  }

  const positiveOffset = Math.max(offsetY, 0)
  const scale = getVerticalGestureScale(positiveOffset)
  const fadeProgress = getVerticalGestureFadeProgress(positiveOffset)

  surface.style.transform = `translate3d(0, ${positiveOffset}px, 0) scale(${scale})`
  surface.style.opacity = isClosing.value ? '0' : String(1 - fadeProgress * 0.28)
  surface.style.transition = isClosing.value
    ? 'none'
    : phase === 'dragging'
      ? 'none'
      : `transform ${VERTICAL_GESTURE_SNAPBACK_MS}ms cubic-bezier(0.16, 1, 0.3, 1), opacity ${VERTICAL_GESTURE_SNAPBACK_MS}ms ease-out`
  surface.style.willChange = phase === 'idle' ? 'auto' : 'transform, opacity'
}

function clearVerticalGestureSurfaceStyles() {
  const surface = mediaGestureSurfaceRef.value
  if (!surface) {
    return
  }

  surface.style.removeProperty('transform')
  surface.style.removeProperty('opacity')
  surface.style.removeProperty('transition')
  surface.style.removeProperty('will-change')
}

function queueVerticalGestureSurfaceUpdate(offsetY: number) {
  pendingVerticalGestureOffset = offsetY
  if (verticalGestureRafId !== null) {
    return
  }

  verticalGestureRafId = requestAnimationFrame(() => {
    verticalGestureRafId = null
    const pendingOffset = pendingVerticalGestureOffset
    pendingVerticalGestureOffset = null
    if (pendingOffset !== null && verticalGesturePhase === 'dragging') {
      applyVerticalGestureSurfaceStyle(pendingOffset, 'dragging')
    }
  })
}

function clearVerticalGestureResetTimer() {
  if (verticalGestureResetTimer !== null) {
    window.clearTimeout(verticalGestureResetTimer)
    verticalGestureResetTimer = null
  }
}

function resetVerticalGestureSurface() {
  clearVerticalGestureResetTimer()
  clearVerticalGestureFrame()
  pendingExitGestureOffset = null
  verticalGestureOffset = 0
  verticalGesturePhase = 'idle'
  clearVerticalGestureSurfaceStyles()
}

function settleVerticalGestureBack(offsetY: number) {
  clearVerticalGestureResetTimer()
  clearVerticalGestureFrame()
  verticalGestureOffset = offsetY
  verticalGesturePhase = 'settling'
  applyVerticalGestureSurfaceStyle(offsetY, 'settling')

  verticalGestureRafId = requestAnimationFrame(() => {
    verticalGestureRafId = null
    if (verticalGesturePhase !== 'settling') {
      return
    }

    verticalGestureOffset = 0
    applyVerticalGestureSurfaceStyle(0, 'settling')
    verticalGestureResetTimer = window.setTimeout(() => {
      verticalGestureResetTimer = null
      if (verticalGestureOffset === 0 && verticalGesturePhase === 'settling') {
        verticalGesturePhase = 'idle'
        clearVerticalGestureSurfaceStyles()
      }
    }, VERTICAL_GESTURE_SNAPBACK_MS)
  })
}

function handleVerticalGestureMove(offsetY: number) {
  clearVerticalGestureResetTimer()
  verticalGestureOffset = offsetY
  verticalGesturePhase = 'dragging'
  queueVerticalGestureSurfaceUpdate(offsetY)
}

function handleVerticalGestureCancel(offsetY: number) {
  settleVerticalGestureBack(offsetY)
}

function handleVerticalGestureCommit(action: LightboxVerticalGestureAction, offsetY: number) {
  clearVerticalGestureResetTimer()

  if (action === 'details') {
    // 上滑打开详情时先让媒体回到原位，抽屉由 MobileDrawer 自己执行进入动画。
    settleVerticalGestureBack(offsetY)
    handleToolbarToggleDetails()
    return
  }

  pendingExitGestureOffset = Math.max(offsetY, 0)
  // 保留松手时的媒体位置，等退场 Hero 以同一个几何状态接管，避免中间再发生一次位移。
  clearVerticalGestureFrame()
  verticalGesturePhase = 'settling'
  verticalGestureOffset = Math.max(offsetY, 0)
  applyVerticalGestureSurfaceStyle(verticalGestureOffset, 'settling')
  requestClose()
}

watch(isClosing, (closing) => {
  if (!closing || (verticalGesturePhase === 'idle' && verticalGestureOffset === 0)) {
    return
  }

  // 只有垂直手势触发的关闭才需要用内联透明度覆盖媒体容器的 opacity-0 class。
  applyVerticalGestureSurfaceStyle(verticalGestureOffset, 'settling')
})

// 工具栏、背景点击和 Escape 共用这条入口，确保关闭动作同步消费暗房历史。
function requestClose() {
  if (isGalleryLightboxOverlay(overlayHistory.snapshot.value.overlay)) {
    void overlayHistory.closeLightbox()
    return
  }

  animateClose()
}

function handleToolbarFit() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxPagerRef.value?.showFitMode()
}

function handleToolbarActual() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxPagerRef.value?.showActualSize()
}

function handleToolbarZoomIn() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxPagerRef.value?.zoomIn()
}

function handleToolbarZoomOut() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxPagerRef.value?.zoomOut()
}

function handleToolbarRotate(deltaDegrees: number) {
  if (!isZoomableAsset.value) {
    return
  }
  lightbox.rotateView(deltaDegrees)
}

function handleToolbarToggleFilmstrip() {
  lightbox.toggleFilmstrip()
}

// 紧凑工具栏的详情按钮使用独立历史层，不影响暗房本身的返回层级。
function handleToolbarToggleDetails() {
  if (!isToolbarCompressed.value) {
    return
  }

  if (mobileDetailsOpen.value) {
    // 已打开时回退一层，保留暗房本身的历史项。
    closeMobileDetails()
    return
  }

  // 未打开时新增详情层，让系统返回手势只关闭详情抽屉。
  const assetId = currentAsset.value?.id ?? store.selection.activeAssetId
  void overlayHistory.openLightboxDetails(assetId)
}

// 详情关闭只消费详情层，不直接关闭暗房。
function closeMobileDetails() {
  void overlayHistory.closeLightboxDetails()
}

function clearPendingTouchTap() {
  if (pendingTouchTapTimer !== null) {
    window.clearTimeout(pendingTouchTapTimer)
    pendingTouchTapTimer = null
  }
}

function toggleLightboxChrome() {
  const visible = !store.lightbox.chromeVisible
  store.setLightboxChromeVisible(visible)
  if (!visible) {
    closeMobileDetails()
  }
}

function scheduleSingleTouchTap() {
  clearPendingTouchTap()
  pendingTouchTapTimer = window.setTimeout(() => {
    pendingTouchTapTimer = null
    if (!store.lightbox.isOpen || isClosing.value || !canToggleChromeByTap.value) {
      return
    }

    toggleLightboxChrome()
  }, TOUCH_SINGLE_TAP_DELAY_MS)
}

// 视频画面单击仍需先让原生播放按钮完成 click，再切换图库 chrome，避免 controls 提前卸载。
function scheduleVideoChromeToggle() {
  clearPendingTouchTap()
  pendingTouchTapTimer = window.setTimeout(() => {
    pendingTouchTapTimer = null
    if (!store.lightbox.isOpen || isClosing.value || !canToggleChromeByTap.value) {
      return
    }

    toggleLightboxChrome()
  }, 0)
}

function handleTouchTap(isDoubleTap: boolean) {
  if (isClosing.value) {
    clearPendingTouchTap()
    return
  }

  if (!canToggleChromeByTap.value) {
    clearPendingTouchTap()
    return
  }

  // 视频不参与双击缩放；单击先让原生播放器处理，再切换图库 chrome。
  if (!isZoomableAsset.value) {
    scheduleVideoChromeToggle()
    return
  }

  if (isDoubleTap) {
    clearPendingTouchTap()
    // Pager 已经完成双击缩放；双击在窄屏触摸下同时进入纯图片状态。
    store.setLightboxChromeVisible(false)
    closeMobileDetails()
    return
  }

  scheduleSingleTouchTap()
}

function handleToolbarToggleImmersive() {
  if (isImmersive.value) {
    exitImmersive()
  } else {
    enterImmersive()
  }
}

function handleImageContextMenu(event: MouseEvent) {
  const asset = currentAsset.value
  const currentIndex = store.selection.activeIndex
  if (!asset || currentIndex === undefined) {
    return
  }

  void gallerySelection.handleAssetContextMenu(asset, event, currentIndex)
}

function handleMediaContextMenu(event: MouseEvent) {
  if (isGalleryTouchContextMenu(event, activeInputType.value)) {
    // ContextMenuTrigger 仍保持挂载，但触摸长按不应打开桌面右键菜单。
    event.preventDefault()
    event.stopPropagation()
    return
  }

  handleImageContextMenu(event)
}

function handleMediaWheel(event: WheelEvent) {
  if (!store.lightbox.isOpen || !currentAsset.value || event.deltaY === 0) {
    return
  }

  if (isZoomableAsset.value && (event.ctrlKey || fitMode.value === 'actual')) {
    return
  }

  event.preventDefault()

  if (event.deltaY > 0) {
    lightbox.goToNext()
  } else {
    lightbox.goToPrevious()
  }
}

function handleKeydown(event: KeyboardEvent) {
  if (!store.lightbox.isOpen || isEditableTarget(event.target)) {
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.shiftKey) {
    const key = event.key.toLowerCase()
    if (key === 'c') {
      event.preventDefault()
      void assetActions.copySelectedAssetTags()
      return
    }
    if (key === 'v') {
      event.preventDefault()
      void assetActions.pasteCopiedTagsToSelection()
      return
    }
  }

  if (
    (event.ctrlKey || event.metaKey) &&
    !event.shiftKey &&
    // 灯箱普通 Ctrl+C 会写入宿主机剪贴板，远端应保留浏览器默认行为。
    isLocalAccess() &&
    event.key.toLowerCase() === 'c' &&
    store.selection.selectedIds.size > 0
  ) {
    event.preventDefault()
    void assetActions.handleCopyAssetsToClipboard()
    return
  }

  switch (event.key) {
    case 'ArrowLeft':
      event.preventDefault()
      throttledPrevious()
      return
    case 'ArrowRight':
      event.preventDefault()
      throttledNext()
      return
    case 'Escape':
      event.preventDefault()
      // Escape 按覆盖层层级退出：详情抽屉 -> 沉浸模式 -> 暗房。
      if (mobileDetailsOpen.value) {
        closeMobileDetails()
        return
      }
      if (isImmersive.value) {
        exitImmersive()
        return
      }
      requestClose()
      return
    case 'f':
    case 'F':
      event.preventDefault()
      handleToolbarToggleImmersive()
      return
    case 'Tab':
      event.preventDefault()
      handleToolbarToggleFilmstrip()
      return
    case '0':
      event.preventDefault()
      // 让 0~5 与 Lightroom 的审片习惯保持一致；缩放切换改由 Z 负责。
      void assetActions.clearSelectedAssetsRating()
      return
    case '1':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(1)
      return
    case '2':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(2)
      return
    case '3':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(3)
      return
    case '4':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(4)
      return
    case '5':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(5)
      return
    case 'z':
    case 'Z':
      event.preventDefault()
      if (isZoomableAsset.value) {
        lightbox.toggleFitActual()
      }
      return
    case 'x':
    case 'X':
      event.preventDefault()
      if (currentAsset.value?.reviewFlag === 'rejected') {
        void assetActions.clearSelectedAssetsRejected()
      } else {
        void assetActions.setSelectedAssetsRejected()
      }
      return
    case '=':
    case '+':
      event.preventDefault()
      if (isZoomableAsset.value) {
        throttledZoomIn()
      }
      return
    case '-':
    case '_':
      event.preventDefault()
      if (isZoomableAsset.value) {
        throttledZoomOut()
      }
      return
    default:
      if (event.code === 'NumpadAdd' && isZoomableAsset.value) {
        event.preventDefault()
        throttledZoomIn()
      } else if (event.code === 'NumpadSubtract' && isZoomableAsset.value) {
        event.preventDefault()
        throttledZoomOut()
      }
  }
}

const heroOverlay = ref<{ thumbnailUrl: string } | null>(null)
const heroOverlayStyle = ref<Record<string, string>>({})
let heroRafId: number | null = null

const reverseHeroOverlay = ref<{ thumbnailUrl: string } | null>(null)
const reverseHeroOverlayStyle = ref<Record<string, string>>({})
let reverseHeroRafId: number | null = null

function startEnterHero() {
  const hero = consumeHero()
  if (!hero) {
    endHeroAnimation()
    return
  }

  const heroViewport = getHeroViewport()
  if (!heroViewport) {
    endHeroAnimation()
    return
  }

  const toRect = computeLightboxHeroRect(
    heroViewport.rect,
    hero.width,
    hero.height,
    heroViewport.padding
  )

  heroOverlay.value = { thumbnailUrl: hero.thumbnailUrl }
  heroOverlayStyle.value = rectToFixedStyle(hero.rect, 'none')

  heroRafId = requestAnimationFrame(() => {
    heroRafId = requestAnimationFrame(() => {
      heroOverlayStyle.value = rectToFixedStyle(toRect, 'enter')
    })
  })
}

function onHeroTransitionEnd() {
  heroOverlay.value = null
  endHeroAnimation()
}

function startExitReverseHero(fromRect: DOMRect, toRect: DOMRect, thumbnailUrl: string) {
  reverseHeroOverlay.value = { thumbnailUrl }
  reverseHeroOverlayStyle.value = rectToFixedStyle(fromRect, 'none')

  reverseHeroRafId = requestAnimationFrame(() => {
    reverseHeroRafId = requestAnimationFrame(() => {
      reverseHeroOverlayStyle.value = rectToFixedStyle(toRect, 'exit')
    })
  })
}

function onReverseHeroTransitionEnd() {
  reverseHeroOverlay.value = null
}

onMounted(async () => {
  await nextTick()
  startEnterHero()
})

useEventListener(window, 'keydown', handleKeydown)
onUnmounted(() => {
  clearPendingTouchTap()
  clearDetailsContentTimer()
  clearVerticalGestureResetTimer()
  clearVerticalGestureFrame()
  pendingExitGestureOffset = null
  clearVerticalGestureSurfaceStyles()
  endHeroAnimation()
  if (heroRafId !== null) cancelAnimationFrame(heroRafId)
  if (reverseHeroRafId !== null) cancelAnimationFrame(reverseHeroRafId)
})
</script>

<template>
  <Teleport to="body" :disabled="!isImmersive">
    <div
      ref="lightboxRootRef"
      class="lightbox-container"
      :class="lightboxRootClass"
      style="--surface-opacity-scale: 0.96"
      @click.self="requestClose"
      @pointerdown.capture="handleLightboxPointerDown"
    >
      <div
        class="relative h-full min-h-0 w-full"
        :class="isReservedDesktopLayout ? 'flex flex-col' : ''"
      >
        <Transition
          appear
          enter-active-class="transition-opacity duration-[200ms] ease-out"
          enter-from-class="opacity-0"
          enter-to-class="opacity-100"
          leave-active-class="transition-opacity duration-[160ms] ease-in"
          leave-from-class="opacity-100"
          leave-to-class="opacity-0"
        >
          <div
            v-if="isLightboxChromeVisible && !isClosing"
            class="pointer-events-auto z-30"
            :class="isReservedDesktopLayout ? 'relative shrink-0' : 'absolute inset-x-0 top-0'"
          >
            <LightboxToolbar
              :compressed="isToolbarCompressed"
              :details-open="mobileDetailsOpen"
              @back="requestClose"
              @fit="handleToolbarFit"
              @actual="handleToolbarActual"
              @zoom-in="handleToolbarZoomIn"
              @zoom-out="handleToolbarZoomOut"
              @rotate="handleToolbarRotate"
              @toggle-filmstrip="handleToolbarToggleFilmstrip"
              @toggle-details="handleToolbarToggleDetails"
              @toggle-immersive="handleToolbarToggleImmersive"
            />
          </div>
        </Transition>

        <div
          ref="mediaViewportRef"
          class="min-h-0 min-w-0"
          :class="
            isReservedDesktopLayout
              ? 'relative flex-1 overflow-hidden'
              : 'absolute inset-0 overflow-hidden'
          "
        >
          <!-- Enter hero overlay 正在放大的图片处于媒体视口内 (z-10)，处于上下渐变栏下方 -->
          <img
            v-if="heroOverlay"
            :src="heroOverlay.thumbnailUrl"
            :style="heroOverlayStyle"
            class="pointer-events-none fixed z-10 rounded-[4px] object-cover"
            alt=""
            @transitionend="onHeroTransitionEnd"
          />

          <!-- Exit reverse hero overlay 正在飞回缩略图的图片也处于媒体视口内 (z-10)，处于上下渐变栏下方 -->
          <img
            v-if="reverseHeroOverlay"
            :src="reverseHeroOverlay.thumbnailUrl"
            :style="reverseHeroOverlayStyle"
            class="pointer-events-none fixed z-10 rounded-[4px] object-cover"
            alt=""
            @transitionend="onReverseHeroTransitionEnd"
          />

          <ContextMenu v-if="currentAsset">
            <ContextMenuTrigger as-child :disabled="isTouchInput">
              <div
                ref="mediaGestureSurfaceRef"
                class="absolute inset-0 z-0 overflow-hidden transition-opacity duration-[180ms]"
                :class="isClosing ? 'opacity-0' : 'opacity-100'"
                @contextmenu.capture="handleMediaContextMenu"
                @wheel="handleMediaWheel"
              >
                <!-- Pager 负责媒体轨道，按钮保持在轨道外，避免随页面一起移动。 -->
                <LightboxPager
                  ref="lightboxPagerRef"
                  :vertical-gesture-enabled="canUseVerticalGesture"
                  :touch-chrome-enabled="canToggleChromeByTap"
                  @touch-tap="handleTouchTap"
                  @vertical-gesture-move="handleVerticalGestureMove"
                  @vertical-gesture-cancel="handleVerticalGestureCancel"
                  @vertical-gesture-commit="handleVerticalGestureCommit"
                />
                <LightboxNavigationButtons
                  :can-previous="canGoToPrevious"
                  :can-next="canGoToNext"
                  @previous="throttledPrevious"
                  @next="throttledNext"
                />
              </div>
            </ContextMenuTrigger>
            <ContextMenuContent class="w-56">
              <GalleryAssetContextMenuContent />
            </ContextMenuContent>
          </ContextMenu>
          <div
            v-else
            ref="mediaGestureSurfaceRef"
            class="absolute inset-0 z-0 overflow-hidden transition-opacity duration-[180ms]"
            :class="isClosing ? 'opacity-0' : 'opacity-100'"
            @contextmenu.prevent.stop
            @wheel="handleMediaWheel"
          >
            <LightboxPager
              ref="lightboxPagerRef"
              :vertical-gesture-enabled="canUseVerticalGesture"
              :touch-chrome-enabled="canToggleChromeByTap"
              @touch-tap="handleTouchTap"
              @vertical-gesture-move="handleVerticalGestureMove"
              @vertical-gesture-cancel="handleVerticalGestureCancel"
              @vertical-gesture-commit="handleVerticalGestureCommit"
            />
            <LightboxNavigationButtons
              :can-previous="canGoToPrevious"
              :can-next="canGoToNext"
              @previous="throttledPrevious"
              @next="throttledNext"
            />
          </div>
        </div>

        <!-- 普通桌面模式让底部 chrome 占据空间；紧凑/沉浸模式仍将其覆盖在媒体上。 -->
        <div
          class="pointer-events-none z-30 flex flex-col-reverse"
          :class="isReservedDesktopLayout ? 'relative shrink-0' : 'absolute inset-x-0 bottom-0'"
        >
          <Transition
            appear
            enter-active-class="transition-opacity duration-[200ms] ease-out"
            enter-from-class="opacity-0"
            enter-to-class="opacity-100"
            leave-active-class="transition-opacity duration-[160ms] ease-in"
            leave-from-class="opacity-100"
            leave-to-class="opacity-0"
          >
            <div
              v-if="isLightboxChromeVisible && isTouchInput && !isClosing"
              class="pointer-events-auto shrink-0"
            >
              <GalleryMobileActionBar />
            </div>
          </Transition>

          <Transition
            appear
            enter-active-class="transition-all duration-300"
            enter-from-class="translate-y-full opacity-0"
            enter-to-class="translate-y-0 opacity-100"
            leave-active-class="transition-all duration-300"
            leave-from-class="translate-y-0 opacity-100"
            leave-to-class="translate-y-full opacity-0"
          >
            <div
              v-if="isLightboxChromeVisible && showFilmstrip && !isClosing"
              class="pointer-events-auto shrink-0"
            >
              <LightboxFilmstrip />
            </div>
          </Transition>
        </div>

        <MobileDrawer
          :open="isToolbarCompressed && mobileDetailsOpen && !isClosing"
          side="bottom"
          :z-index="110"
          :close-on-escape="false"
          class="h-[82vh] max-h-[720px] rounded-t-2xl border-t border-border bg-background pb-[env(safe-area-inset-bottom)] text-sidebar-foreground"
          @close="closeMobileDetails"
        >
          <div class="flex h-11 shrink-0 items-center justify-between border-b px-4">
            <h2 class="text-sm font-medium text-foreground">{{ t('gallery.details.title') }}</h2>
            <Button
              variant="ghost"
              size="icon"
              class="h-10 w-10"
              :aria-label="t('gallery.lightbox.toolbar.closeTitle')"
              @click="closeMobileDetails"
            >
              <X class="h-5 w-5" />
            </Button>
          </div>

          <div class="min-h-0 flex-1">
            <div
              v-if="!detailsContentReady"
              class="flex h-full items-center justify-center p-6"
              aria-live="polite"
            >
              <div class="w-full max-w-sm space-y-3" aria-hidden="true">
                <div class="h-5 w-1/3 animate-pulse rounded bg-muted" />
                <div class="h-40 animate-pulse rounded-lg bg-muted/70" />
                <div class="space-y-2">
                  <div class="h-3 animate-pulse rounded bg-muted/70" />
                  <div class="h-3 w-4/5 animate-pulse rounded bg-muted/70" />
                </div>
              </div>
            </div>
            <GalleryDetails v-else :defer-secondary-details="true" />
          </div>
        </MobileDrawer>
      </div>
    </div>
  </Teleport>
</template>
