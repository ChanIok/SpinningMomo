<script setup lang="ts">
import { computed } from 'vue'
import { useGalleryStore } from '../store'
import GridView from './GridView.vue'
import ListView from './ListView.vue'
import MasonryView from './MasonryView.vue'
import AdaptiveView from './AdaptiveView.vue'

const store = useGalleryStore()
const viewMode = computed(() => store.viewConfig.mode)
</script>

<template>
  <div class="h-full w-full">
    <GridView v-if="viewMode === 'grid'" />
    <ListView v-else-if="viewMode === 'list'" />
    <MasonryView v-else-if="viewMode === 'masonry'" />
    <AdaptiveView v-else />
  </div>
</template>