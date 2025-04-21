<template>
  <div
    class="cursor-pointer transition-colors duration-200 bg-transparent rounded overflow-hidden"
    :class="{ 'outline-2 outline-primary': selected }"
    @click="handleClick"
  >
    <div class="relative w-full aspect-square bg-background hover:bg-background-hover group">
      <img
        :src="imageUrl"
        :alt="screenshot.filename"
        loading="lazy"
        class="w-full h-full object-cover"
      />
      <div
        class="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/75 to-transparent p-3 text-white opacity-0 transition-opacity duration-200 group-hover:opacity-100"
      >
        <div class="flex flex-col gap-1">
          <div class="text-sm font-medium truncate">{{ screenshot.filename }}</div>
          <div class="flex gap-2 text-xs opacity-90">
            <span>{{ formatDate(screenshot.created_at) }}</span>
            <span>{{ formatFileSize(screenshot.file_size) }}</span>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { Screenshot } from '@/types/screenshot'

const props = defineProps<{
  screenshot: Screenshot
  selected?: boolean
}>()

const emit = defineEmits<{
  (e: 'click', screenshot: Screenshot): void
}>()

const imageUrl = computed(() => `/api/screenshots/${props.screenshot.id}/thumbnail`)

function handleClick() {
  emit('click', props.screenshot)
}

function formatDate(date: string | number): string {
  const timestamp = typeof date === 'string' ? parseInt(date) : date
  const dateObj = new Date(timestamp * 1000)
  return `${dateObj.toLocaleDateString()} ${dateObj.toLocaleTimeString()}`
}

function formatFileSize(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB']
  let size = bytes
  let unitIndex = 0

  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024
    unitIndex++
  }

  return `${size.toFixed(1)} ${units[unitIndex]}`
}
</script>
