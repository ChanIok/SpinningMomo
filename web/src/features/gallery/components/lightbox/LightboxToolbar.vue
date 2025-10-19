<script setup lang="ts">
import { computed } from 'vue'
import { useGalleryStore } from '../../store'
import { useGalleryLightbox } from '../../composables'

const store = useGalleryStore()
const lightbox = useGalleryLightbox()

const currentIndex = computed(() => store.lightbox.currentIndex)
const totalCount = computed(() => store.totalCount)
const selectedCount = computed(() => store.selection.selectedIds.size)
const showFilmstrip = computed(() => store.lightbox.showFilmstrip)
const isFullscreen = computed(() => store.lightbox.isFullscreen)

function handleClose() {
  lightbox.closeLightbox()
}

function handleToggleFullscreen() {
  lightbox.toggleFullscreen()
}

function handleToggleFilmstrip() {
  lightbox.toggleFilmstrip()
}
</script>

<template>
  <div
    class="flex items-center justify-between border-b border-border bg-card/80 px-4 py-3 backdrop-blur-sm"
  >
    <!-- 左侧：图片计数 -->
    <div class="flex items-center gap-3 text-foreground">
      <span class="text-sm font-medium"> {{ currentIndex + 1 }} / {{ totalCount }} </span>
      <span v-if="selectedCount > 0" class="text-xs text-primary"> 已选 {{ selectedCount }} </span>
    </div>

    <!-- 右侧：控制按钮 -->
    <div class="flex items-center gap-2">
      <!-- 切换Filmstrip按钮 -->
      <button
        class="inline-flex items-center justify-center rounded-md p-2 text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="handleToggleFilmstrip"
        :title="showFilmstrip ? '隐藏缩略图 (Tab)' : '显示缩略图 (Tab)'"
      >
        <svg
          v-if="showFilmstrip"
          class="h-5 w-5"
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M19 9l-7 7-7-7"
          />
        </svg>
        <svg v-else class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5 15l7-7 7 7" />
        </svg>
      </button>

      <!-- 全屏按钮 -->
      <button
        class="inline-flex items-center justify-center rounded-md p-2 text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="handleToggleFullscreen"
        :title="isFullscreen ? '退出全屏 (F)' : '全屏 (F)'"
      >
        <svg
          v-if="isFullscreen"
          class="h-5 w-5"
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M6 18L18 6M6 6l12 12"
          />
        </svg>
        <svg v-else class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4"
          />
        </svg>
      </button>

      <!-- 关闭按钮 -->
      <button
        class="inline-flex items-center justify-center rounded-md p-2 text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
        @click="handleClose"
        title="关闭 (ESC)"
      >
        <svg class="h-5 w-5" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M6 18L18 6M6 6l12 12"
          />
        </svg>
      </button>
    </div>
  </div>
</template>
