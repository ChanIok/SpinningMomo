<script setup lang="ts">
import { computed, ref } from 'vue'
import { useGalleryStore } from '../store'
import GridView from './GridView.vue'
import ListView from './ListView.vue'
import MasonryView from './MasonryView.vue'
import AdaptiveView from './AdaptiveView.vue'

const store = useGalleryStore()
const viewMode = computed(() => store.viewConfig.mode)

interface GalleryViewExposed {
  scrollToIndex: (index: number) => void
  getCardRect: (index: number) => DOMRect | null
}

const gridViewRef = ref<GalleryViewExposed | null>(null)
const listViewRef = ref<GalleryViewExposed | null>(null)
const masonryViewRef = ref<GalleryViewExposed | null>(null)

function scrollToIndex(index: number) {
  if (viewMode.value === 'grid') gridViewRef.value?.scrollToIndex(index)
  else if (viewMode.value === 'list') listViewRef.value?.scrollToIndex(index)
  else if (viewMode.value === 'masonry') masonryViewRef.value?.scrollToIndex(index)
}

function getCardRect(index: number): DOMRect | null {
  if (viewMode.value === 'grid') return gridViewRef.value?.getCardRect(index) ?? null
  if (viewMode.value === 'list') return listViewRef.value?.getCardRect(index) ?? null
  if (viewMode.value === 'masonry') return masonryViewRef.value?.getCardRect(index) ?? null
  return null
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div class="h-full w-full">
    <GridView v-if="viewMode === 'grid'" ref="gridViewRef" />
    <ListView v-else-if="viewMode === 'list'" ref="listViewRef" />
    <MasonryView v-else-if="viewMode === 'masonry'" ref="masonryViewRef" />
    <AdaptiveView v-else />
  </div>
</template>
