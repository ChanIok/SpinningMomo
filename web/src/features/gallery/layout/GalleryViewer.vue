<script setup lang="ts">
import { watch } from 'vue'
import { useEventListener } from '@vueuse/core'
import { useGalleryAssetActions, useGalleryData } from '../composables'
import { useGalleryStore } from '../store'
import GalleryToolbar from '../components/GalleryToolbar.vue'
import GalleryContent from '../components/GalleryContent.vue'
import GalleryLightbox from '../components/lightbox/GalleryLightbox.vue'

const galleryData = useGalleryData()
const store = useGalleryStore()
const assetActions = useGalleryAssetActions()

function toggleSelectedAssetsRejected() {
  const activeIndex = store.selection.activeIndex
  const activeAsset =
    activeIndex === undefined ? null : (store.getAssetsInRange(activeIndex, activeIndex)[0] ?? null)

  if (activeAsset?.reviewFlag === 'rejected') {
    void assetActions.clearSelectedAssetsRejected()
    return
  }

  void assetActions.setSelectedAssetsRejected()
}

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

function handleKeydown(event: KeyboardEvent) {
  if (
    store.lightbox.isOpen ||
    isEditableTarget(event.target) ||
    store.selection.selectedIds.size === 0
  ) {
    return
  }

  switch (event.key) {
    case '0':
      event.preventDefault()
      void assetActions.clearSelectedAssetsRating()
      return
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(Number(event.key))
      return
    case 'x':
    case 'X':
      event.preventDefault()
      toggleSelectedAssetsRejected()
      return
  }
}

// 监听筛选条件和文件夹选项变化，自动重新加载资产
watch(
  () => [store.filter, store.includeSubfolders, store.sortBy, store.sortOrder],
  async () => {
    console.log('🔄 筛选条件变化，重新加载数据')
    await galleryData.refreshCurrentQuery()
  },
  { deep: true }
)

useEventListener(window, 'keydown', handleKeydown)
</script>

<template>
  <div class="h-full">
    <!-- 当lightbox打开时，只显示lightbox -->
    <GalleryLightbox v-if="store.lightbox.isOpen" />

    <!-- 当lightbox关闭时，显示正常的工具栏和内容区域 -->
    <div v-else class="flex h-full flex-col">
      <GalleryToolbar />
      <div class="flex-1 overflow-hidden">
        <GalleryContent />
      </div>
    </div>
  </div>
</template>
