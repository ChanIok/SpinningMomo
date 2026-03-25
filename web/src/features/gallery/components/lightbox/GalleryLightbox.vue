<script setup lang="ts">
import { computed, ref } from 'vue'
import { useEventListener, useThrottleFn } from '@vueuse/core'
import { useI18n } from '@/composables/useI18n'
import { useGalleryAssetActions, useGalleryLightbox, useGallerySelection } from '../../composables'
import { useGalleryStore } from '../../store'
import { prepareReverseHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import GalleryAssetContextMenuContent from '../GalleryAssetContextMenuContent.vue'
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
const isClosing = ref(false)

const isImmersive = computed(() => store.lightbox.isImmersive)
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
const lightboxContainerClass = computed(() =>
  isImmersive.value
    ? 'surface-bottom fixed inset-0 z-[100] flex overflow-hidden shadow-2xl'
    : 'absolute inset-0 z-10 flex h-full w-full overflow-hidden'
)

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
  if (isClosing.value) return
  isClosing.value = true

  // gallery 始终以 visibility:hidden 保持渲染和正确滚动位置，可直接同步读取 cardRect
  const activeIndex = store.selection.activeIndex
  if (activeIndex !== undefined) {
    const galleryContent = props.galleryContentRef
    if (galleryContent) {
      const cardRect = galleryContent.getCardRect(activeIndex)
      const asset = store.getAssetsInRange(activeIndex, activeIndex)[0]
      if (cardRect && asset) {
        const TOOLBAR_HEIGHT = 61
        const FILMSTRIP_HEIGHT = store.lightbox.showFilmstrip ? 101 : 0
        const VIEWPORT_PADDING = 32
        const W = document.documentElement.clientWidth
        const H = document.documentElement.clientHeight
        const contentW = W - VIEWPORT_PADDING * 2
        const contentH = H - TOOLBAR_HEIGHT - FILMSTRIP_HEIGHT - VIEWPORT_PADDING * 2
        const imgW = asset.width ?? 1
        const imgH = asset.height ?? 1
        const scale = Math.min(contentW / imgW, contentH / imgH, 1)
        const targetW = imgW * scale
        const targetH = imgH * scale
        const fromRect = new DOMRect(
          (W - targetW) / 2,
          TOOLBAR_HEIGHT + (contentH + VIEWPORT_PADDING * 2 - targetH) / 2,
          targetW,
          targetH
        )
        prepareReverseHero(fromRect, cardRect, galleryApi.getAssetThumbnailUrl(asset))
        emit('requestReverseHero')
      }
    }
  }

  setTimeout(() => {
    isClosing.value = false
    lightbox.closeLightbox()
  }, 220)
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
      class="lightbox-container"
      :class="lightboxContainerClass"
      style="--surface-opacity-scale: 0.96"
      @click.self="handleClose"
    >
      <div class="flex h-full min-h-0 w-full flex-col">
        <Transition
          appear
          enter-active-class="transition-all duration-[220ms] ease-out delay-[40ms]"
          enter-from-class="opacity-0 -translate-y-3"
          enter-to-class="opacity-100 translate-y-0"
          leave-active-class="transition-all duration-[180ms] ease-in"
          leave-from-class="opacity-100 translate-y-0"
          leave-to-class="opacity-0 -translate-y-3"
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
              <ContextMenuItem @click="handleToolbarFit">
                {{ t('gallery.lightbox.contextMenu.fit') }}
              </ContextMenuItem>
              <ContextMenuItem @click="handleToolbarActual">
                {{ t('gallery.lightbox.contextMenu.actual') }}
              </ContextMenuItem>
              <ContextMenuItem @click="handleToolbarZoomIn">
                {{ t('gallery.lightbox.contextMenu.zoomIn') }}
              </ContextMenuItem>
              <ContextMenuItem @click="handleToolbarZoomOut">
                {{ t('gallery.lightbox.contextMenu.zoomOut') }}
              </ContextMenuItem>
              <ContextMenuSeparator />
            </template>
            <ContextMenuItem @click="handleToolbarToggleFilmstrip">
              {{
                showFilmstrip
                  ? t('gallery.lightbox.contextMenu.hideFilmstrip')
                  : t('gallery.lightbox.contextMenu.showFilmstrip')
              }}
            </ContextMenuItem>
            <ContextMenuItem @click="handleToolbarToggleImmersive">
              {{
                isImmersive
                  ? t('gallery.lightbox.contextMenu.exitImmersive')
                  : t('gallery.lightbox.contextMenu.immersive')
              }}
            </ContextMenuItem>
            <ContextMenuSeparator />
            <GalleryAssetContextMenuContent />
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
          enter-active-class="transition-all duration-300 delay-[80ms]"
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
