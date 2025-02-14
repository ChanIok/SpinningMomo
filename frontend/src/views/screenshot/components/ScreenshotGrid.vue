<script setup lang="ts">
import { NGrid, NGridItem, NSpin } from 'naive-ui'
import type { Screenshot } from '@/types/screenshot'

const props = defineProps<{
  screenshots: Screenshot[]
  loading: boolean
  hasMore: boolean
}>()

const emit = defineEmits<{
  (e: 'load-more'): void
}>()
</script>

<template>
  <div class="screenshot-grid">
    <n-grid
      :x-gap="16"
      :y-gap="16"
      cols="2 s:3 m:4 l:5 xl:6 2xl:8"
      responsive="screen"
    >
      <n-grid-item v-for="screenshot in props.screenshots" :key="screenshot.id">
        <div class="screenshot-item">
          <img 
            :src="screenshot.thumbnailPath" 
            :alt="screenshot.filename"
            loading="lazy"
          />
        </div>
      </n-grid-item>
    </n-grid>

    <div v-if="props.loading || props.hasMore" class="loading-more">
      <n-spin v-if="props.loading" size="small" />
      <button 
        v-else-if="props.hasMore" 
        class="load-more-btn"
        @click="emit('load-more')"
      >
        加载更多
      </button>
    </div>
  </div>
</template>

<style scoped>
.screenshot-grid {
  padding: 16px;
}

.screenshot-item {
  aspect-ratio: 1;
  overflow: hidden;
  border-radius: 8px;
  background-color: var(--n-card-color);
}

.screenshot-item img {
  width: 100%;
  height: 100%;
  object-fit: cover;
  transition: transform 0.2s ease;
}

.screenshot-item:hover img {
  transform: scale(1.05);
}

.loading-more {
  display: flex;
  justify-content: center;
  padding: 24px 0;
}

.load-more-btn {
  padding: 8px 16px;
  border: none;
  border-radius: 4px;
  background-color: var(--n-primary-color);
  color: white;
  cursor: pointer;
  transition: opacity 0.2s ease;
}

.load-more-btn:hover {
  opacity: 0.9;
}
</style> 