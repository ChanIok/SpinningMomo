<script setup lang="ts">
import { computed, ref } from 'vue'
import { useEventListener, useThrottleFn } from '@vueuse/core'
import { useGalleryLightbox } from '../../composables'
import { useGalleryStore } from '../../store'
import LightboxFilmstrip from './LightboxFilmstrip.vue'
import LightboxImage from './LightboxImage.vue'
import LightboxToolbar from './LightboxToolbar.vue'

type LightboxImageExposed = {
  showFitMode: () => Promise<void>
  showActualSize: () => Promise<void>
  zoomIn: () => Promise<void>
  zoomOut: () => Promise<void>
}

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const lightboxImageRef = ref<LightboxImageExposed | null>(null)

// 键盘事件统一在此父组件注册一次，避免子组件重复绑定
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

const showFilmstrip = computed(() => store.lightbox.showFilmstrip)

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

function handleBackdropClick() {
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
      lightbox.closeLightbox()
      return
    case 'f':
    case 'F':
      event.preventDefault()
      lightbox.toggleFullscreen()
      return
    case 'Tab':
      event.preventDefault()
      lightbox.toggleFilmstrip()
      return
    case '0':
      event.preventDefault()
      handleToolbarFit()
      return
    case '1':
      event.preventDefault()
      handleToolbarActual()
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
useEventListener(document, 'fullscreenchange', () => {
  if (!document.fullscreenElement && store.lightbox.isFullscreen) {
    store.toggleLightboxFullscreen()
  }
})
</script>

<template>
  <Transition
    enter-active-class="transition-opacity duration-300"
    enter-from-class="opacity-0"
    enter-to-class="opacity-100"
    leave-active-class="transition-opacity duration-300"
    leave-from-class="opacity-100"
    leave-to-class="opacity-0"
  >
    <div
      class="lightbox-container flex h-full w-full overflow-hidden"
      style="--surface-opacity-scale: 0.95"
      @click.self="handleBackdropClick"
    >
      <div class="flex h-full w-full flex-col">
        <LightboxToolbar
          @fit="handleToolbarFit"
          @actual="handleToolbarActual"
          @zoom-in="handleToolbarZoomIn"
          @zoom-out="handleToolbarZoomOut"
        />

        <div class="min-h-0 flex-1">
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
  </Transition>
</template>
