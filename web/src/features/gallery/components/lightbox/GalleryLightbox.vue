<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, ref } from 'vue'
import { useEventListener, useThrottleFn } from '@vueuse/core'
import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { useI18n } from '@/composables/useI18n'
import { useGalleryAssetActions, useGalleryLightbox, useGallerySelection } from '../../composables'
import { useGalleryStore } from '../../store'
import GalleryAssetContextMenuContent from '../GalleryAssetContextMenuContent.vue'
import LightboxFilmstrip from './LightboxFilmstrip.vue'
import LightboxImage from './LightboxImage.vue'
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

type WebViewFullscreenResult = {
  success: boolean
  fullscreen: boolean
}

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const gallerySelection = useGallerySelection()
const assetActions = useGalleryAssetActions()
const { t } = useI18n()
const lightboxRootRef = ref<HTMLElement | null>(null)
const lightboxImageRef = ref<LightboxImageExposed | null>(null)

const isFullscreen = computed(() => store.lightbox.isFullscreen)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)
const currentAsset = computed(() => {
  const currentIndex = store.selection.activeIndex
  if (currentIndex === undefined) {
    return null
  }

  return store.getAssetsInRange(currentIndex, currentIndex)[0]
})
const lightboxContainerClass = computed(() =>
  isFullscreen.value
    ? 'surface-bottom fixed inset-0 z-[100] flex overflow-hidden shadow-2xl'
    : 'flex h-full w-full overflow-hidden'
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

async function syncNativeFullscreen(fullscreen: boolean) {
  if (!isWebView()) {
    return
  }

  try {
    const result = await call<WebViewFullscreenResult>('webview.setFullscreen', { fullscreen })
    lightbox.setFullscreen(result.fullscreen)
  } catch (error) {
    console.warn(`Failed to ${fullscreen ? 'enter' : 'exit'} WebView fullscreen:`, error)
  }
}

async function requestBrowserFullscreen() {
  const root = lightboxRootRef.value
  if (!root?.requestFullscreen) {
    return
  }

  try {
    await root.requestFullscreen()
  } catch (error) {
    console.warn('Failed to enter browser fullscreen:', error)
  }
}

async function exitBrowserFullscreen() {
  if (!document.fullscreenElement || !document.exitFullscreen) {
    return
  }

  try {
    await document.exitFullscreen()
  } catch (error) {
    console.warn('Failed to exit browser fullscreen:', error)
  }
}

async function enterFullscreenPresentation() {
  if (isFullscreen.value) {
    return
  }

  lightbox.setFullscreen(true)
  await nextTick()

  if (isWebView()) {
    await syncNativeFullscreen(true)
    return
  }

  await requestBrowserFullscreen()
}

async function exitFullscreenPresentation() {
  if (!isFullscreen.value && !document.fullscreenElement) {
    return
  }

  if (isWebView()) {
    await syncNativeFullscreen(false)
  } else {
    await exitBrowserFullscreen()
  }

  lightbox.setFullscreen(false)
}

async function handleClose() {
  await exitFullscreenPresentation()
  lightbox.closeLightbox()
}

function handleToolbarFit() {
  void lightboxImageRef.value?.showFitMode()
}

function handleToolbarActual() {
  void lightboxImageRef.value?.showActualSize()
}

function handleToolbarZoomIn() {
  void lightboxImageRef.value?.zoomIn()
}

function handleToolbarZoomOut() {
  void lightboxImageRef.value?.zoomOut()
}

function handleToolbarToggleFilmstrip() {
  lightbox.toggleFilmstrip()
}

function handleToolbarToggleFullscreen() {
  void (isFullscreen.value ? exitFullscreenPresentation() : enterFullscreenPresentation())
}

function handleImageContextMenu(event: MouseEvent) {
  const asset = currentAsset.value
  const currentIndex = store.selection.activeIndex
  if (!asset || currentIndex === undefined) {
    return
  }

  void gallerySelection.handleAssetContextMenu(asset, event, currentIndex)
}

function handleDocumentFullscreenChange() {
  if (isWebView()) {
    return
  }

  if (!document.fullscreenElement && isFullscreen.value) {
    lightbox.setFullscreen(false)
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
      if (isFullscreen.value || document.fullscreenElement) {
        void exitFullscreenPresentation()
        return
      }
      void handleClose()
      return
    case 'f':
    case 'F':
      event.preventDefault()
      handleToolbarToggleFullscreen()
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
      lightbox.toggleFitActual()
      return
    case 'p':
    case 'P':
      event.preventDefault()
      void assetActions.setSelectedAssetsReviewFlag('picked')
      return
    case 'x':
    case 'X':
      event.preventDefault()
      void assetActions.setSelectedAssetsReviewFlag('rejected')
      return
    case 'u':
    case 'U':
      event.preventDefault()
      void assetActions.clearSelectedAssetsReviewFlag()
      return
    case '=':
    case '+':
      event.preventDefault()
      throttledZoomIn()
      return
    case '-':
    case '_':
      event.preventDefault()
      throttledZoomOut()
      return
    default:
      if (event.code === 'NumpadAdd') {
        event.preventDefault()
        throttledZoomIn()
      } else if (event.code === 'NumpadSubtract') {
        event.preventDefault()
        throttledZoomOut()
      }
  }
}

useEventListener(window, 'keydown', handleKeydown)
useEventListener(document, 'fullscreenchange', handleDocumentFullscreenChange)

onBeforeUnmount(() => {
  if (isWebView()) {
    void syncNativeFullscreen(false)
  } else if (document.fullscreenElement) {
    void exitBrowserFullscreen()
  }
})
</script>

<template>
  <Teleport to="body" :disabled="!isFullscreen">
    <div
      ref="lightboxRootRef"
      class="lightbox-container"
      :class="lightboxContainerClass"
      style="--surface-opacity-scale: 0.96"
      @click.self="handleClose"
    >
      <div class="flex h-full min-h-0 w-full flex-col">
        <LightboxToolbar
          @back="handleClose"
          @fit="handleToolbarFit"
          @actual="handleToolbarActual"
          @zoom-in="handleToolbarZoomIn"
          @zoom-out="handleToolbarZoomOut"
          @toggle-filmstrip="handleToolbarToggleFilmstrip"
          @toggle-fullscreen="handleToolbarToggleFullscreen"
        />

        <ContextMenu v-if="currentAsset">
          <ContextMenuTrigger as-child>
            <div class="min-h-0 flex-1" @contextmenu="handleImageContextMenu">
              <LightboxImage ref="lightboxImageRef" />
            </div>
          </ContextMenuTrigger>
          <ContextMenuContent class="w-56">
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
            <ContextMenuItem @click="handleToolbarToggleFilmstrip">
              {{
                showFilmstrip
                  ? t('gallery.lightbox.contextMenu.hideFilmstrip')
                  : t('gallery.lightbox.contextMenu.showFilmstrip')
              }}
            </ContextMenuItem>
            <ContextMenuItem @click="handleToolbarToggleFullscreen">
              {{
                isFullscreen
                  ? t('gallery.lightbox.contextMenu.exitFullscreen')
                  : t('gallery.lightbox.contextMenu.fullscreen')
              }}
            </ContextMenuItem>
            <ContextMenuSeparator />
            <GalleryAssetContextMenuContent />
          </ContextMenuContent>
        </ContextMenu>
        <div v-else class="min-h-0 flex-1">
          <LightboxImage ref="lightboxImageRef" />
        </div>

        <Transition
          enter-active-class="transition-all duration-300"
          enter-from-class="translate-y-full opacity-0"
          enter-to-class="translate-y-0 opacity-100"
          leave-active-class="transition-all duration-300"
          leave-from-class="translate-y-0 opacity-100"
          leave-to-class="translate-y-full opacity-0"
        >
          <LightboxFilmstrip v-if="showFilmstrip" />
        </Transition>
      </div>
    </div>
  </Teleport>
</template>
