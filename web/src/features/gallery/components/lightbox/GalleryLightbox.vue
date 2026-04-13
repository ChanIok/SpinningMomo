<script setup lang="ts">
import { computed, ref } from 'vue'
import { useEventListener, useThrottleFn } from '@vueuse/core'
import { Maximize } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useGalleryAssetActions, useGalleryLightbox, useGallerySelection } from '../../composables'
import { useGalleryStore } from '../../store'
import { computeLightboxHeroRect, prepareReverseHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import GalleryAssetContextMenuContent from '../menus/GalleryAssetContextMenuContent.vue'
import LightboxFilmstrip from './LightboxFilmstrip.vue'
import LightboxImage from './LightboxImage.vue'
import LightboxVideo from './LightboxVideo.vue'
import LightboxToolbar from './LightboxToolbar.vue'
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuTrigger,
} from '@/components/ui/context-menu'

/** 与反向 hero、surface 淡出时长（约 220ms）对齐，并留出双 rAF 余量 */
const CLOSE_AFTER_REVERSE_HERO_MS = 260
/** 无飞回动画时，与工具栏/内容区 leave ~180ms 对齐 */
const CLOSE_AFTER_NO_HERO_MS = 180

type LightboxImageExposed = {
  showFitMode: () => Promise<void>
  showActualSize: () => Promise<void>
  zoomIn: () => Promise<void>
  zoomOut: () => Promise<void>
}

type GalleryContentRef = {
  scrollToIndex: (index: number) => void
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
const assetActions = useGalleryAssetActions()
const { t } = useI18n()
const lightboxImageRef = ref<LightboxImageExposed | null>(null)
const lightboxRootRef = ref<HTMLElement | null>(null)

const isImmersive = computed(() => store.lightbox.isImmersive)
const isClosing = computed(() => store.lightbox.isClosing)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)
const fitMode = computed(() => store.lightbox.fitMode)
const currentAsset = computed(() => {
  const currentIndex = store.selection.activeIndex
  if (currentIndex === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIndex, currentIndex)[0]
})
// 缩放、适屏、1:1 都是静态图查看语义；视频在灯箱里保持原生播放器行为。
const isZoomableAsset = computed(() => currentAsset.value?.type !== 'video')
const lightboxRootClass = computed(() => {
  const immersive = isImmersive.value
  const closing = store.lightbox.isClosing
  let cls = immersive
    ? 'surface-bottom fixed inset-0 z-[100] flex overflow-hidden shadow-2xl'
    : 'absolute inset-0 z-10 flex h-full w-full overflow-hidden'
  if (immersive && closing) {
    cls += ' pointer-events-none opacity-0 transition-opacity duration-[280ms] ease-out'
  }
  return cls
})

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
    void lightboxImageRef.value?.zoomIn()
  }
}, 60)

const throttledZoomOut = useThrottleFn(() => {
  if (store.lightbox.isOpen) {
    void lightboxImageRef.value?.zoomOut()
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

function handleClose() {
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
        const fromRect = computeLightboxHeroRect(
          containerRect,
          asset.width ?? 1,
          asset.height ?? 1,
          store.lightbox.showFilmstrip
        )
        prepareReverseHero(fromRect, cardRect, galleryApi.getAssetThumbnailUrl(asset))
        emit('requestReverseHero')
        didReverseHero = true
      }
    }
  }

  const delay = didReverseHero ? CLOSE_AFTER_REVERSE_HERO_MS : CLOSE_AFTER_NO_HERO_MS
  window.setTimeout(() => {
    lightbox.closeLightbox()
  }, delay)
}

function handleToolbarFit() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxImageRef.value?.showFitMode()
}

function handleToolbarActual() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxImageRef.value?.showActualSize()
}

function handleToolbarZoomIn() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxImageRef.value?.zoomIn()
}

function handleToolbarZoomOut() {
  if (!isZoomableAsset.value) {
    return
  }
  void lightboxImageRef.value?.zoomOut()
}

function handleToolbarToggleFilmstrip() {
  lightbox.toggleFilmstrip()
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
      if (isImmersive.value) {
        exitImmersive()
        return
      }
      handleClose()
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
</script>

<template>
  <Teleport to="body" :disabled="!isImmersive">
    <div
      ref="lightboxRootRef"
      class="lightbox-container"
      :class="lightboxRootClass"
      style="--surface-opacity-scale: 0.96"
      @click.self="handleClose"
    >
      <div class="flex h-full min-h-0 w-full flex-col">
        <Transition
          appear
          enter-active-class="transition-opacity duration-[200ms] ease-out"
          enter-from-class="opacity-0"
          enter-to-class="opacity-100"
          leave-active-class="transition-opacity duration-[160ms] ease-in"
          leave-from-class="opacity-100"
          leave-to-class="opacity-0"
        >
          <LightboxToolbar
            v-if="!isClosing"
            @back="handleClose"
            @fit="handleToolbarFit"
            @actual="handleToolbarActual"
            @zoom-in="handleToolbarZoomIn"
            @zoom-out="handleToolbarZoomOut"
            @toggle-filmstrip="handleToolbarToggleFilmstrip"
            @toggle-immersive="handleToolbarToggleImmersive"
          />
        </Transition>

        <ContextMenu v-if="currentAsset">
          <ContextMenuTrigger as-child>
            <div
              class="min-h-0 flex-1 transition-opacity duration-[180ms]"
              :class="isClosing ? 'opacity-0' : 'opacity-100'"
              @contextmenu="handleImageContextMenu"
              @wheel="handleMediaWheel"
            >
              <LightboxImage v-if="isZoomableAsset" ref="lightboxImageRef" />
              <LightboxVideo v-else @previous="throttledPrevious" @next="throttledNext" />
            </div>
          </ContextMenuTrigger>
          <ContextMenuContent class="w-56">
            <template v-if="isZoomableAsset">
              <GalleryAssetContextMenuContent />
              <ContextMenuSeparator />
              <ContextMenuItem inset @click="handleToolbarFit">
                {{ t('gallery.lightbox.contextMenu.fit') }}
              </ContextMenuItem>
              <ContextMenuItem @click="handleToolbarActual">
                <Maximize class="size-4 text-muted-foreground" />
                {{ t('gallery.lightbox.contextMenu.actual') }}
              </ContextMenuItem>
              <ContextMenuItem inset @click="handleToolbarZoomIn">
                {{ t('gallery.lightbox.contextMenu.zoomIn') }}
              </ContextMenuItem>
              <ContextMenuItem inset @click="handleToolbarZoomOut">
                {{ t('gallery.lightbox.contextMenu.zoomOut') }}
              </ContextMenuItem>
              <ContextMenuSeparator />
            </template>
            <ContextMenuItem inset @click="handleToolbarToggleFilmstrip">
              {{
                showFilmstrip
                  ? t('gallery.lightbox.contextMenu.hideFilmstrip')
                  : t('gallery.lightbox.contextMenu.showFilmstrip')
              }}
            </ContextMenuItem>
            <ContextMenuItem inset @click="handleToolbarToggleImmersive">
              {{
                isImmersive
                  ? t('gallery.lightbox.contextMenu.exitImmersive')
                  : t('gallery.lightbox.contextMenu.immersive')
              }}
            </ContextMenuItem>
          </ContextMenuContent>
        </ContextMenu>
        <div
          v-else
          class="min-h-0 flex-1 transition-opacity duration-[180ms]"
          :class="isClosing ? 'opacity-0' : 'opacity-100'"
        >
          <LightboxImage ref="lightboxImageRef" />
        </div>

        <Transition
          appear
          enter-active-class="transition-all duration-300"
          enter-from-class="translate-y-full opacity-0"
          enter-to-class="translate-y-0 opacity-100"
          leave-active-class="transition-all duration-300"
          leave-from-class="translate-y-0 opacity-100"
          leave-to-class="translate-y-full opacity-0"
        >
          <LightboxFilmstrip v-if="showFilmstrip && !isClosing" />
        </Transition>
      </div>
    </div>
  </Teleport>
</template>
