<script setup lang="ts">
import { NList, NListItem, NSpin } from 'naive-ui'
import type { Screenshot } from '@/types/screenshot'

const props = defineProps<{
  screenshots: Screenshot[]
  loading: boolean
  hasMore: boolean
}>()

const emit = defineEmits<{
  (e: 'load-more'): void
}>()

function formatFileSize(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

function formatDate(timestamp: number): string {
  return new Date(timestamp).toLocaleString()
}
</script>

<template>
  <div class="screenshot-list">
    <n-list>
      <n-list-item v-for="screenshot in props.screenshots" :key="screenshot.id">
        <div class="list-item-content">
          <div class="thumbnail">
            <img 
              :src="screenshot.thumbnailPath" 
              :alt="screenshot.filename"
              loading="lazy"
            />
          </div>
          <div class="info">
            <h3>{{ screenshot.filename }}</h3>
            <p class="details">
              <span>{{ formatFileSize(screenshot.file_size) }}</span>
              <span>{{ screenshot.width }} x {{ screenshot.height }}</span>
              <span>{{ formatDate(screenshot.created_at) }}</span>
            </p>
          </div>
        </div>
      </n-list-item>
    </n-list>

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
.screenshot-list {
  padding: 16px;
}

.list-item-content {
  display: flex;
  gap: 16px;
  align-items: center;
}

.thumbnail {
  width: 120px;
  height: 68px;
  overflow: hidden;
  border-radius: 4px;
  flex-shrink: 0;
}

.thumbnail img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.info {
  flex: 1;
}

.info h3 {
  margin: 0 0 8px;
  font-size: 16px;
  color: var(--n-text-color);
}

.details {
  margin: 0;
  font-size: 14px;
  color: var(--n-text-color-3);
  display: flex;
  gap: 16px;
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