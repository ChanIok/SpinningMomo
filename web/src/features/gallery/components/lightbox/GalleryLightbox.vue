<script setup lang="ts">
import { computed, watch } from 'vue'
import { onKeyStroke, useThrottleFn } from '@vueuse/core'
import { useGalleryStore } from '../../store'
import { useGalleryLightbox } from '../../composables'
import LightboxToolbar from './LightboxToolbar.vue'
import LightboxImage from './LightboxImage.vue'
import LightboxFilmstrip from './LightboxFilmstrip.vue'

const store = useGalleryStore()
const lightbox = useGalleryLightbox()

// 键盘事件监听（只在父组件注册一次）
const throttledPrevious = useThrottleFn(() => {
  if (store.lightbox.isOpen) lightbox.goToPrevious()
}, 200)

const throttledNext = useThrottleFn(() => {
  if (store.lightbox.isOpen) lightbox.goToNext()
}, 200)

onKeyStroke('ArrowLeft', throttledPrevious)
onKeyStroke('ArrowRight', throttledNext)
onKeyStroke('Escape', () => {
  if (store.lightbox.isOpen) lightbox.closeLightbox()
})
onKeyStroke(['f', 'F'], (e) => {
  if (store.lightbox.isOpen) {
    e.preventDefault()
    lightbox.toggleFullscreen()
  }
})
onKeyStroke('Tab', (e) => {
  if (store.lightbox.isOpen) {
    e.preventDefault()
    lightbox.toggleFilmstrip()
  }
})

const isFullscreen = computed(() => store.lightbox.isFullscreen)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)

// 点击背景关闭
function handleBackdropClick() {
  // 可选：点击背景关闭lightbox
  lightbox.closeLightbox()
}

// 监听全屏状态变化
watch(isFullscreen, (newValue) => {
  if (newValue) {
    document.documentElement.requestFullscreen?.()
  } else {
    if (document.fullscreenElement) {
      document.exitFullscreen?.()
    }
  }
})

// 监听原生全屏事件（用户按ESC退出全屏）
watch(
  () => document.fullscreenElement,
  (element) => {
    if (!element && store.lightbox.isFullscreen) {
      store.toggleLightboxFullscreen()
    }
  }
)
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
      class="lightbox-container flex h-full w-full overflow-hidden bg-background/95"
      @click.self="handleBackdropClick"
    >
      <!-- Lightbox内容 -->
      <div class="flex h-full w-full flex-col">
        <!-- 顶部工具栏 -->
        <LightboxToolbar />

        <!-- 主图片区域 -->
        <div class="min-h-0 flex-1">
          <LightboxImage />
        </div>

        <!-- 底部Filmstrip -->
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
