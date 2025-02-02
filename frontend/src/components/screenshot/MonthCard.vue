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
        v-if="stats.firstScreenshot"
        :src="stats.firstScreenshot.thumbnailPath"
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
  background: #fff;
  border-radius: 8px;
  overflow: hidden;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  cursor: pointer;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}

.month-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
}

.cover-image {
  aspect-ratio: 16/9;
  background: #f5f5f5;
  overflow: hidden;
}

.cover-image :deep(img) {
  width: 100%;
  height: 100%;
  object-fit: cover;
  transition: transform 0.3s ease;
}

.month-card:hover .cover-image :deep(img) {
  transform: scale(1.05);
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