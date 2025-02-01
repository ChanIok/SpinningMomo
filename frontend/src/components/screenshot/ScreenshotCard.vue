<template>
  <n-card
    class="screenshot-card"
    :class="{ selected }"
    hoverable
    @click="handleClick"
  >
    <n-image
      :src="imageUrl"
      :alt="screenshot.filename"
      object-fit="cover"
      preview-disabled
      class="screenshot-image"
    />
    <template #footer>
      <div class="screenshot-info">
        <n-ellipsis>{{ screenshot.filename }}</n-ellipsis>
        <div class="screenshot-metadata">
          <span>{{ formatDate(screenshot.created_at) }}</span>
          <span>{{ formatFileSize(screenshot.file_size) }}</span>
        </div>
      </div>
    </template>
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

const imageUrl = computed(() => `/api/screenshots/${props.screenshot.id}/raw`);

function handleClick() {
  emit('click', props.screenshot);
}

function formatDate(date: string): string {
  return new Date(date).toLocaleDateString();
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
}

.screenshot-card:hover {
  transform: translateY(-2px);
}

.screenshot-card.selected {
  border: 2px solid var(--n-primary-color);
}

.screenshot-image {
  width: 100%;
  aspect-ratio: 16/9;
  background-color: var(--n-card-color);
}

.screenshot-info {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.screenshot-metadata {
  display: flex;
  gap: 8px;
  font-size: 0.85em;
  color: rgba(0, 0, 0, 0.45);
}
</style> 