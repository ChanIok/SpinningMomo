<script setup lang="ts">
import { ref, computed, watch } from 'vue'
import { useGalleryStore } from '../../store'
import { useGalleryLightbox } from '../../composables'
import { galleryApi } from '../../api'

const store = useGalleryStore()
const lightbox = useGalleryLightbox()

const isLoading = ref(true)
const imageError = ref(false)

const currentAsset = computed(() => {
  const { assets, currentIndex } = store.lightbox
  return assets[currentIndex]
})

const imageUrl = computed(() => {
  if (!currentAsset.value) return ''
  // TODO: 等待后端原图API
  // return galleryApi.getAssetOriginalUrl(currentAsset.value)

  // 暂时使用缩略图
  return galleryApi.getAssetThumbnailUrl(currentAsset.value)
})

const canGoToPrevious = computed(() => store.lightbox.currentIndex > 0)
const canGoToNext = computed(() => store.lightbox.currentIndex < store.lightbox.assets.length - 1)

// 监听当前资产变化，重置加载状态
watch(currentAsset, () => {
  isLoading.value = true
  imageError.value = false
})

function handleImageLoad() {
  isLoading.value = false
  imageError.value = false
}

function handleImageError() {
  isLoading.value = false
  imageError.value = true
}

function handlePrevious() {
  lightbox.goToPrevious()
}

function handleNext() {
  lightbox.goToNext()
}

function handleImageClick() {
  // 可以添加点击图片的交互，比如切换工具栏显示
}

</script>

<template>
  <div class="relative flex h-full w-full items-center justify-center bg-background">
    <!-- 左侧导航按钮 -->
    <button
      v-if="canGoToPrevious"
      class="absolute left-4 z-10 inline-flex h-12 w-12 items-center justify-center rounded-full bg-card/80 text-foreground backdrop-blur-sm transition-all hover:bg-card"
      @click="handlePrevious"
      title="上一张 (←)"
    >
      <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 19l-7-7 7-7" />
      </svg>
    </button>

    <!-- 主图片区域 -->
    <div class="relative flex h-full w-full items-center justify-center p-8">
      <img
        v-if="currentAsset && !imageError"
        :src="imageUrl"
        :alt="currentAsset.name"
        class="max-h-full max-w-full object-contain select-none"
        @load="handleImageLoad"
        @error="handleImageError"
        @click.self="handleImageClick"
      />

      <!-- 加载状态 -->
      <div
        v-if="isLoading"
        class="absolute inset-0 flex items-center justify-center bg-card/50 backdrop-blur-sm"
      >
        <div class="flex flex-col items-center gap-3 text-foreground">
          <div class="h-12 w-12 animate-spin rounded-full border-b-2 border-foreground"></div>
          <span class="text-sm">加载中...</span>
        </div>
      </div>

      <!-- 错误状态 -->
      <div
        v-if="imageError"
        class="flex flex-col items-center justify-center text-muted-foreground"
      >
        <svg class="mb-4 h-16 w-16" fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path
            stroke-linecap="round"
            stroke-linejoin="round"
            stroke-width="2"
            d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
          />
        </svg>
        <p class="text-lg">图片加载失败</p>
        <p class="mt-2 text-sm text-muted-foreground/70">{{ currentAsset?.name }}</p>
      </div>
    </div>

    <!-- 右侧导航按钮 -->
    <button
      v-if="canGoToNext"
      class="absolute right-4 z-10 inline-flex h-12 w-12 items-center justify-center rounded-full bg-card/80 text-foreground backdrop-blur-sm transition-all hover:bg-card"
      @click="handleNext"
      title="下一张 (→)"
    >
      <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7" />
      </svg>
    </button>
  </div>
</template>
