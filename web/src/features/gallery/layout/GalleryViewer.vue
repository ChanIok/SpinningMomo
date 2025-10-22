<script setup lang="ts">
import { watch } from 'vue'
import { useGalleryData } from '../composables'
import { useGalleryStore } from '../store'
import GalleryToolbar from '../components/GalleryToolbar.vue'
import GalleryContent from '../components/GalleryContent.vue'
import GalleryLightbox from '../components/lightbox/GalleryLightbox.vue'

const galleryData = useGalleryData()
const store = useGalleryStore()

// 监听筛选条件和文件夹选项变化，自动重新加载资产
watch(
  () => [
    store.filter,
    store.includeSubfolders,
    store.sortBy,
    store.sortOrder,
  ],
  async () => {
    console.log('🔄 筛选条件变化，重新加载数据')
    // 根据当前模式选择合适的加载方法
    if (store.isTimelineMode) {
      await galleryData.loadTimelineData()
    } else {
      await galleryData.loadAllAssets()
    }
  },
  { deep: true }
)
</script>

<template>
  <div class="h-full">
    <!-- 当lightbox打开时，只显示lightbox -->
    <GalleryLightbox v-if="store.lightbox.isOpen" />

    <!-- 当lightbox关闭时，显示正常的工具栏和内容区域 -->
    <div v-else class="flex h-full flex-col bg-background">
      <GalleryToolbar />
      <div class="flex-1 overflow-hidden">
        <GalleryContent />
      </div>
    </div>
  </div>
</template>
