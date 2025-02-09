<script setup lang="ts">
import { NImage, NButton, NIcon } from 'naive-ui'
import { CreateOutline as EditIcon } from '@vicons/ionicons5'
import type { Album } from '@/types/album'
import { useAlbumSelectionStore } from '@/stores/album-selection'
import { CheckmarkCircle, RadioButtonOff } from '@vicons/ionicons5'

const props = defineProps<{
  album: Album
}>()

const albumSelectionStore = useAlbumSelectionStore()

const emit = defineEmits<{
  (e: 'click'): void
  (e: 'edit'): void
}>()

// 阻止编辑按钮点击事件冒泡到卡片
function handleEditClick(e: Event) {
  e.stopPropagation()
  emit('edit')
}

// 处理选择按钮点击
function handleSelectButtonClick(e: Event) {
  e.stopPropagation()
  if (!albumSelectionStore.isSelectionMode) {
    albumSelectionStore.enterSelectionMode()
  }
  albumSelectionStore.toggleSelection(props.album.id)
}
</script>

<template>
  <div 
    class="album-card" 
    @click="$emit('click')"
    :class="{ 'is-selected': albumSelectionStore.isSelected(album.id) }"
  >
    <!-- 选择按钮 -->
    <n-tooltip trigger="hover">
      <template #trigger>
        <div
          class="selection-button"
          :class="{ 
            'is-visible': albumSelectionStore.isSelectionMode,
            'is-selected': albumSelectionStore.isSelected(album.id)
          }"
          @click="handleSelectButtonClick"
        >
          <n-icon size="24">
            <CheckmarkCircle v-if="albumSelectionStore.isSelected(album.id)" />
            <RadioButtonOff v-else />
          </n-icon>
        </div>
      </template>
      {{ albumSelectionStore.isSelectionMode ? '选择/取消选择' : '开始选择' }}
    </n-tooltip>

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
  position: relative;
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

.selection-button {
  position: absolute;
  top: 8px;
  left: 8px;
  z-index: 2;
  opacity: 0;
  transition: all 0.2s ease;
  background: rgba(255, 255, 255, 0.9);
  border-radius: 50%;
  width: 28px;
  height: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;

  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  color: #8a8a8a;
}

.selection-button:hover {
  background: white;
  transform: scale(1.1);
  color: var(--primary-color);
}

.selection-button.is-selected {
  color: var(--primary-color);
  background: white;
}

.album-card:hover .selection-button {
  opacity: 1;
}

.selection-button.is-visible {
  opacity: 1;
}

.album-card.is-selected {
  outline: 2px solid var(--primary-color);
  outline-offset: -2px;
}

.album-card.is-selected .cover-image :deep(img) {
  opacity: 0.9;
}
</style> 