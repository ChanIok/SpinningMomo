<script setup lang="ts">
import { watch } from 'vue'
import { useGalleryData } from '../composables'
import { useGalleryStore } from '../store'
import GalleryToolbar from '../components/GalleryToolbar.vue'
import GalleryContent from '../components/GalleryContent.vue'

const galleryData = useGalleryData()
const store = useGalleryStore()

// 监听筛选条件和文件夹选项变化，自动重新加载资产
watch(
  () => [
    store.filter.folderId,
    store.filter.type,
    store.filter.searchQuery,
    store.includeSubfolders,
    store.sortBy,
    store.sortOrder,
  ],
  async () => {
    console.log('🔄 筛选条件变化，重新加载数据')
    // 使用统一加载方法，自动根据模式选择
    await galleryData.load()
  },
  { deep: true }
)
</script>

<template>
  <div class="flex h-full flex-col bg-background">
    <!-- 工具栏 -->
    <GalleryToolbar />

    <!-- 内容区域 -->
    <div class="flex-1 overflow-hidden">
      <GalleryContent />
    </div>
  </div>
</template>
