<script setup lang="ts">
import { ref, computed, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { useGalleryStore } from '../../store'
import { useGalleryLightbox } from '../../composables'
import { useGalleryData } from '../../composables'
import { galleryApi } from '../../api'

const store = useGalleryStore()
const lightbox = useGalleryLightbox()
const galleryData = useGalleryData()

const imageError = ref(false)
const originalLoaded = ref(false)

// 使用 useElementSize 获取容器实际大小
const containerRef = ref<HTMLElement>()
const { width, height } = useElementSize(containerRef)

// 计算可用空间（用于限制图片最大尺寸）
const availableWidth = computed(() => width.value)
const availableHeight = computed(() => height.value)

// 从 virtualizer 获取当前资产
const currentAsset = computed(() => {
  const currentIdx = store.lightbox.currentIndex
  // 直接使用 store.getAssetsInRange 获取当前资产
  return store.getAssetsInRange(currentIdx, currentIdx)[0]
})

// 缩略图URL
const thumbnailUrl = computed(() => {
  if (!currentAsset.value) return ''
  return galleryApi.getAssetThumbnailUrl(currentAsset.value)
})

// 原图URL
const originalUrl = computed(() => {
  if (!currentAsset.value) return ''
  return galleryData.getAssetUrl(currentAsset.value.id)
})

// 获取当前图片加载状态
const imageState = computed(() => {
  if (!currentAsset.value) return { status: 'idle' as const }
  return lightbox.getImageState(currentAsset.value.id)
})

const canGoToPrevious = computed(() => store.lightbox.currentIndex > 0)
const canGoToNext = computed(() => store.lightbox.currentIndex < store.totalCount - 1)

// 监听当前资产变化，重置状态并更新详情面板
watch(currentAsset, (newAsset) => {
  originalLoaded.value = false
  imageError.value = false

  // 更新选中状态和详情面板焦点
  if (newAsset) {
    store.selectAsset(newAsset.id, true, false)
    store.setDetailsFocus({ type: 'asset', asset: newAsset })
  }
})

// 监听原图加载状态
watch(imageState, (state) => {
  if (state.status === 'loaded') {
    originalLoaded.value = true
  } else if (state.status === 'error') {
    imageError.value = true
  }
})

function handleImageError() {
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
  <div ref="containerRef" class="relative flex h-full w-full items-center justify-center">
    <!-- 左侧导航按钮 -->
    <button
      v-if="canGoToPrevious"
      class="surface-top absolute left-4 z-10 inline-flex h-12 w-12 items-center justify-center rounded-full text-foreground transition-all"
      @click="handlePrevious"
      title="上一张 (←)"
    >
      <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 19l-7-7 7-7" />
      </svg>
    </button>

    <!-- 主图片区域 -->
    <div class="flex h-full w-full items-center justify-center p-8">
      <div class="relative grid place-items-center">
        <!-- 缩略图层 -->
        <img
          v-if="currentAsset && !imageError"
          :src="thumbnailUrl"
          :alt="currentAsset.name"
          :style="{
            minWidth: `${currentAsset.width! < availableWidth ? currentAsset.width! : availableWidth}px`,
            minHeight: `${currentAsset.height! < availableHeight ? currentAsset.height! : availableHeight}px`,
            maxWidth: `${availableWidth}px`,
            maxHeight: `${availableHeight}px`,
          }"
          class="col-start-1 row-start-1 object-contain select-none"
          @click.self="handleImageClick"
        />

        <!-- 原图层 -->
        <img
          v-if="currentAsset && !imageError"
          :src="originalUrl"
          :alt="currentAsset.name"
          :style="{
            maxWidth: `${availableWidth}px`,
            maxHeight: `${availableHeight}px`,
            opacity: originalLoaded ? 1 : 0,
          }"
          class="col-start-1 row-start-1 object-contain select-none"
          @error="handleImageError"
          @click.self="handleImageClick"
        />

        <!-- 错误状态 -->
        <div
          v-if="imageError"
          class="col-start-1 row-start-1 flex flex-col items-center justify-center text-muted-foreground"
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
    </div>

    <!-- 右侧导航按钮 -->
    <button
      v-if="canGoToNext"
      class="surface-top absolute right-4 z-10 inline-flex h-12 w-12 items-center justify-center rounded-full text-foreground transition-all"
      @click="handleNext"
      title="下一张 (→)"
    >
      <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7" />
      </svg>
    </button>
  </div>
</template>
