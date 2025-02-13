<script setup lang="ts">
import { NSpace, NButton, NIcon } from 'naive-ui'
import { GridOutline, ListOutline } from '@vicons/ionicons5'

const props = defineProps<{
  mode: 'grid' | 'list'
}>()

const emit = defineEmits<{
  (e: 'update:mode', mode: 'grid' | 'list'): void
}>()
</script>

<template>
  <div class="view-toolbar">
    <n-space align="center" :size="12">
      <div class="view-mode-buttons">
        <n-button
          quaternary
          size="small"
          :type="props.mode === 'grid' ? 'primary' : 'default'"
          @click="emit('update:mode', 'grid')"
        >
          <template #icon>
            <n-icon><grid-outline /></n-icon>
          </template>
        </n-button>
        <n-button
          quaternary
          size="small"
          :type="props.mode === 'list' ? 'primary' : 'default'"
          @click="emit('update:mode', 'list')"
        >
          <template #icon>
            <n-icon><list-outline /></n-icon>
          </template>
        </n-button>
      </div>
    </n-space>
  </div>
</template>

<style scoped>
.view-toolbar {
  flex: none;
  padding: 12px 16px;
  border-bottom: 1px solid var(--n-border-color);
  background-color: var(--n-color);
  position: sticky;
  top: 0;
  z-index: 1;
}

.view-mode-buttons {
  display: flex;
  gap: 4px;
}
</style> 