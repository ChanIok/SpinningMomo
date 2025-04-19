<script setup lang="ts">
import { NImage } from 'naive-ui';
import type { MonthStats } from '@/types/screenshot';

const props = defineProps<{
  stats: MonthStats;
}>();

// 格式化月份显示
function formatMonth(year: number, month: number): string {
  const date = new Date(year, month - 1);
  return new Intl.DateTimeFormat('default', {
    year: 'numeric',
    month: 'long'
  }).format(date);
}
</script>

<template>
  <div class="month-card" @click="$emit('click')">
    <div class="cover-image">
      <n-image
        v-if="stats.first_screenshot_id"
        :src="`/api/screenshots/${stats.first_screenshot_id}/thumbnail`"
        :alt="formatMonth(stats.year, stats.month)"
        object-fit="cover"
        preview-disabled
        lazy
      />
    </div>
    <div class="month-info">
      <h3>{{ formatMonth(stats.year, stats.month) }}</h3>
      <span class="photo-count">{{ stats.count }} photos</span>
    </div>
  </div>
</template>

<style scoped>
.month-card {
  background: var(--n-color);
  border-radius: 4px;
  overflow: hidden;
  border: 1px solid var(--n-border-color);
  cursor: pointer;
  transition: background-color 0.2s ease;
}

.month-card:hover {
  background-color: var(--n-color-hover, rgba(0, 0, 0, 0.03));
}

.cover-image {
  aspect-ratio: 1/1;
  background: #f5f5f5;
  overflow: hidden;
  display: flex;
  justify-content: center;
  align-items: center;
}

.cover-image :deep(.n-image) {
  width: 100%;
  height: 100%;
}

.cover-image :deep(img) {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.month-info {
  padding: 16px;
}

.month-info h3 {
  margin: 0;
  font-size: 1.1em;
  font-weight: 600;
  color: #333;
}

.photo-count {
  display: block;
  margin-top: 4px;
  font-size: 0.9em;
  color: #666;
}
</style>