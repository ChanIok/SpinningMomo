<script setup lang="ts">
import { computed, ref } from 'vue'
import { useGalleryStore } from '../store'
import GridView from './GridView.vue'
import ListView from './ListView.vue'
import MasonryView from './MasonryView.vue'
import AdaptiveView from './AdaptiveView.vue'

const store = useGalleryStore()
const viewMode = computed(() => store.viewConfig.mode)

const gridViewRef = ref<InstanceType<typeof GridView> | null>(null)

function scrollToIndex(index: number) {
  gridViewRef.value?.scrollToIndex(index)
}

function getCardRect(index: number): DOMRect | null {
  return gridViewRef.value?.getCardRect(index) ?? null
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div class="h-full w-full">
    <GridView v-if="viewMode === 'grid'" ref="gridViewRef" />
    <ListView v-else-if="viewMode === 'list'" />
    <MasonryView v-else-if="viewMode === 'masonry'" />
    <AdaptiveView v-else />
  </div>
</template>
