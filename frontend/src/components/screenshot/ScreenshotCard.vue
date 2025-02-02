<template>
  <n-card
    class="screenshot-card"
    :class="{ selected }"
    hoverable
    :bordered="false"
    @click="handleClick"
  >
    <div class="image-container">
      <n-image
        :src="imageUrl"
        :alt="screenshot.filename"
        object-fit="cover"
        preview-disabled
        lazy
        class="screenshot-image"
      />
      <div class="info-overlay">
        <div class="screenshot-info">
          <n-ellipsis class="filename">{{ screenshot.filename }}</n-ellipsis>
          <div class="screenshot-metadata">
            <span>{{ formatDate(screenshot.created_at) }}</span>
            <span>{{ formatFileSize(screenshot.file_size) }}</span>
          </div>
        </div>
      </div>
    </div>
  </n-card>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import type { Screenshot } from '@/types/screenshot';
import { NCard, NEllipsis, NImage } from 'naive-ui';

const props = defineProps<{
  screenshot: Screenshot;
  selected?: boolean;
}>();

const emit = defineEmits<{
  (e: 'click', screenshot: Screenshot): void;
}>();

const imageUrl = computed(() => `/api/screenshots/${props.screenshot.id}/thumbnail`);

function handleClick() {
  emit('click', props.screenshot);
}

function formatDate(date: string | number): string {
  const timestamp = typeof date === 'string' ? parseInt(date) : date;
  const dateObj = new Date(timestamp * 1000);
  return `${dateObj.toLocaleDateString()} ${dateObj.toLocaleTimeString()}`;
}

function formatFileSize(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB'];
  let size = bytes;
  let unitIndex = 0;
  
  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex++;
  }
  
  return `${size.toFixed(1)} ${units[unitIndex]}`;
}
</script>

<style scoped>
.screenshot-card {
  cursor: pointer;
  transition: all 0.2s ease;
  background: transparent;
  border-radius: 4px;
  overflow: hidden;
}

.screenshot-card:hover {
  transform: translateY(-2px);
}

.screenshot-card.selected {
  box-shadow: 0 0 0 2px var(--n-primary-color);
}

.image-container {
  position: relative;
  width: 100%;
  padding-bottom: 66.67%; /* 3:2 aspect ratio */
  background-color: rgba(0, 0, 0, 0.03);
}

.screenshot-image {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
}

.screenshot-image :deep(img) {
  width: 100%;
  height: 100%;
  object-fit: cover;
  object-position: center;
}

.info-overlay {
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  background: linear-gradient(transparent, rgba(0, 0, 0, 0.75));
  padding: 12px;
  color: white;
  opacity: 0;
  transition: opacity 0.2s ease;
}

.screenshot-card:hover .info-overlay {
  opacity: 1;
}

.screenshot-info {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.filename {
  font-size: 0.9em;
  font-weight: 500;
  line-height: 1.4;
}

.screenshot-metadata {
  display: flex;
  gap: 8px;
  font-size: 0.8em;
  opacity: 0.9;
}
</style> 