<script setup lang="ts">
import { computed, onUnmounted, ref, watch } from 'vue'
import { useElementSize, useEventListener, useThrottleFn } from '@vueuse/core'
import { useGalleryAssetActions, useGalleryLightbox, useGallerySelection } from '../../composables'
import {
  isGalleryLightboxOverlay,
  useGalleryOverlayHistory,
} from '../../composables/useGalleryOverlayHistory'
import { useGalleryStore } from '../../store'
import { GALLERY_TOOLBAR_COMPACT_BREAKPOINT } from '../../constants'
import { computeLightboxHeroRect, prepareReverseHero } from '../../composables/useHeroTransition'
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

type LightboxPagerExposed = {
  showFitMode: () => Promise<void>
  showActualSize: () => Promise<void>
  zoomIn: () => Promise<void>
  zoomOut: () => Promise<void>
}

type GalleryContentRef = {
  getCardRect: (index: number) => DOMRect | null
} | null

const props = defineProps<{
  galleryContentRef: GalleryContentRef
}>()

const emit = defineEmits<{
  requestReverseHero: []
}>()

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const gallerySelection = useGallerySelection()
const overlayHistory = useGalleryOverlayHistory()
const assetActions = useGalleryAssetActions()
const lightboxPagerRef = ref<LightboxPagerExposed | null>(null)
const lightboxRootRef = ref<HTMLElement | null>(null)
const { width: lightboxWidth } = useElementSize(lightboxRootRef)
// 详情抽屉是否打开由历史快照决定，浏览器返回和界面按钮都能驱动同一个状态。
const mobileDetailsOpen = computed(
  () => overlayHistory.snapshot.value.overlay === 'lightbox-details'
)
// 界面层显隐与沉浸模式分离；共享状态让全局 App Header 与暗房控件同步。
const isLightboxChromeVisible = computed(() => store.lightbox.chromeVisible)
let pendingTouchTapTimer: number | null = null
// inputType 记录打开来源；这里单独记录会话内最近一次指针模态，支持混合触控设备切换。
const activeInputType = ref<GalleryInputType>('mouse')
const preloadingAssetIds = new Set<number>()
const preloadedAssetIds = new Set<number>()
// 暗房内部宽度只决定顶部工具栏如何压缩，以及详情抽屉是否可用。
const isToolbarCompressed = computed(
  () => lightboxWidth.value > 0 && lightboxWidth.value < GALLERY_TOOLBAR_COMPACT_BREAKPOINT
)
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
const isTouchInput = computed(() => isGalleryTouchInput(activeInputType.value))
// 只有窄屏触摸采用相册式“轻点切换界面”的沉浸交互；宽屏触摸保留工作区控件。
const canToggleChromeByTap = computed(() => store.isCompactWindow && isTouchInput.value)
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

watch(
  () => store.lightbox.isOpen,
  (isOpen) => {
    clearPendingTouchTap()
    if (!isOpen) {
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

  if (store.lightbox.isClosing) return
  store.setLightboxClosing(true)

  let didReverseHero = false
  // gallery 在打开时用 opacity 隐藏但仍可布局，可直接同步读取 cardRect
  const activeIndex = store.selection.activeIndex
  if (activeIndex !== undefined) {
    const galleryContent = props.galleryContentRef
    if (galleryContent) {
      const cardRect = galleryContent.getCardRect(activeIndex)
      const asset = store.getAssetsInRange(activeIndex, activeIndex)[0]
      const containerRect = lightboxRootRef.value?.getBoundingClientRect()
      if (cardRect && asset && containerRect) {
        const fromRect = computeLightboxHeroRect(containerRect, asset.width ?? 1, asset.height ?? 1)
        prepareReverseHero(fromRect, cardRect, galleryApi.getAssetThumbnailUrl(asset))
        emit('requestReverseHero')
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
  void overlayHistory.openLightboxDetails(store.selection.activeAssetId)
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

function handleTouchTap(isDoubleTap: boolean) {
  if (isClosing.value) {
    clearPendingTouchTap()
    return
  }

  if (!canToggleChromeByTap.value) {
    clearPendingTouchTap()
    return
  }

  // 视频控件不参与双击缩放，保留其原有的即时单击响应。
  if (!isZoomableAsset.value) {
    clearPendingTouchTap()
    toggleLightboxChrome()
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

useEventListener(window, 'keydown', handleKeydown)
onUnmounted(clearPendingTouchTap)
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
      <div class="relative h-full min-h-0 w-full">
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
            class="pointer-events-auto absolute inset-x-0 top-0 z-30"
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

        <ContextMenu v-if="currentAsset">
          <ContextMenuTrigger as-child :disabled="isTouchInput">
            <div
              class="absolute inset-0 z-0 overflow-hidden transition-opacity duration-[180ms]"
              :class="isClosing ? 'opacity-0' : 'opacity-100'"
              @contextmenu.capture="handleMediaContextMenu"
              @wheel="handleMediaWheel"
            >
              <!-- Pager 负责媒体轨道，按钮保持在轨道外，避免随页面一起移动。 -->
              <LightboxPager ref="lightboxPagerRef" @touch-tap="handleTouchTap" />
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
          class="absolute inset-0 z-0 overflow-hidden transition-opacity duration-[180ms]"
          :class="isClosing ? 'opacity-0' : 'opacity-100'"
          @contextmenu.prevent.stop
          @wheel="handleMediaWheel"
        >
          <LightboxPager ref="lightboxPagerRef" @touch-tap="handleTouchTap" />
          <LightboxNavigationButtons
            :can-previous="canGoToPrevious"
            :can-next="canGoToNext"
            @previous="throttledPrevious"
            @next="throttledNext"
          />
        </div>

        <!-- 底部 chrome 叠放在媒体上；触摸操作栏位于最底部，胶片栏位于其上方。 -->
        <div class="pointer-events-none absolute inset-x-0 bottom-0 z-30 flex flex-col-reverse">
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

        <Transition name="gallery-mobile-details">
          <div
            v-if="isToolbarCompressed && mobileDetailsOpen && !isClosing"
            class="absolute inset-0 z-40 flex items-end"
            role="dialog"
            aria-modal="true"
            :aria-label="t('gallery.details.title')"
          >
            <button
              type="button"
              class="absolute inset-0 cursor-default bg-black/55"
              :aria-label="t('gallery.lightbox.toolbar.closeTitle')"
              @click="closeMobileDetails"
            />

            <section
              class="relative z-10 flex h-[82vh] max-h-[720px] w-full flex-col rounded-t-2xl border-t border-border bg-background shadow-2xl"
            >
              <div class="shrink-0 border-b px-4 pt-2 pb-3">
                <div class="mx-auto mb-2 h-1 w-10 rounded-full bg-muted-foreground/30" />
                <div class="flex items-center justify-between">
                  <h2 class="text-sm font-medium">{{ t('gallery.details.title') }}</h2>
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
              </div>

              <div class="min-h-0 flex-1">
                <GalleryDetails />
              </div>
            </section>
          </div>
        </Transition>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.gallery-mobile-details-enter-active,
.gallery-mobile-details-leave-active {
  transition: opacity 180ms ease-out;
}

.gallery-mobile-details-enter-from,
.gallery-mobile-details-leave-to {
  opacity: 0;
}

.gallery-mobile-details-enter-active section,
.gallery-mobile-details-leave-active section {
  transition: transform 220ms cubic-bezier(0.22, 1, 0.36, 1);
}

.gallery-mobile-details-enter-from section,
.gallery-mobile-details-leave-to section {
  transform: translateY(100%);
}
</style>
