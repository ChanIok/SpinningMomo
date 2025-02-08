<script setup lang="ts">
import { NImage, NButton, NIcon } from 'naive-ui'
import { CreateOutline as EditIcon } from '@vicons/ionicons5'
import type { Album } from '@/types/album'

const props = defineProps<{
  album: Album
}>()

const emit = defineEmits<{
  (e: 'click'): void
  (e: 'edit'): void
}>()

// 阻止编辑按钮点击事件冒泡到卡片
function handleEditClick(e: Event) {
  e.stopPropagation()
  emit('edit')
}
</script>

<template>
  <div class="album-card" @click="$emit('click')">
    <div class="cover-image">
      <n-image
        v-if="album.cover_screenshot_id"
        :src="`/api/screenshots/${album.cover_screenshot_id}/thumbnail`"
        :alt="album.name"
        object-fit="cover"
        preview-disabled
        lazy
      />
      <div v-else class="no-cover">
        No Cover
      </div>
    </div>
    <div class="album-info">
      <div class="album-header">
        <h3>{{ album.name }}</h3>
        <n-button
          quaternary
          circle
          size="small"
          class="edit-button"
          @click="handleEditClick"
        >
          <template #icon>
            <n-icon>
              <edit-icon />
            </n-icon>
          </template>
        </n-button>
      </div>
      <p v-if="album.description" class="description">{{ album.description }}</p>
    </div>
  </div>
</template>

<style scoped>
.album-card {
  background: #fff;
  border-radius: 8px;
  overflow: hidden;
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.1);
  cursor: pointer;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}

.album-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
}

.cover-image {
  aspect-ratio: 1/1;
  background: #f5f5f5;
  overflow: hidden;
  display: flex;
  justify-content: center;
  align-items: center;
}

.no-cover {
  width: 100%;
  height: 100%;
  display: flex;
  justify-content: center;
  align-items: center;
  color: #999;
  font-size: 0.9em;
}

.cover-image :deep(.n-image) {
  width: 100%;
  height: 100%;
}

.cover-image :deep(img) {
  width: 100%;
  height: 100%;
  object-fit: cover;
  transition: transform 0.3s ease;
}

.album-card:hover .cover-image :deep(img) {
  transform: scale(1.05);
}

.album-info {
  padding: 16px;
}

.album-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  gap: 8px;
}

.album-header h3 {
  margin: 0;
  font-size: 1.1em;
  font-weight: 600;
  color: #333;
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.description {
  margin: 8px 0 0;
  font-size: 0.9em;
  color: #666;
  overflow: hidden;
  text-overflow: ellipsis;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
}

.edit-button {
  opacity: 0;
  transition: opacity 0.2s ease;
}

.album-card:hover .edit-button {
  opacity: 1;
}
</style> 