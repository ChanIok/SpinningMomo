<script setup lang="ts">
import { ref } from 'vue'
import { Button } from '@/components/ui/button'
import type { FolderTreeNode } from '../types'

interface Props {
  folder: FolderTreeNode
  selectedFolder: number | null
  depth?: number
}

const props = withDefaults(defineProps<Props>(), {
  depth: 0,
})

const emit = defineEmits<{
  select: [folderId: number, folderName: string]
}>()

// 展开状态
const isExpanded = ref(false)

// 切换展开状态（独立点击箭头）
function toggleExpand() {
  isExpanded.value = !isExpanded.value
}

// 处理 item 点击
function handleItemClick() {
  const isCurrentlySelected = props.selectedFolder === props.folder.id
  const hasChildren = props.folder.children && props.folder.children.length > 0

  if (isCurrentlySelected && hasChildren) {
    // 已选中 + 有子项 → 切换展开
    isExpanded.value = !isExpanded.value
  } else {
    // 未选中 → 选中
    emit('select', props.folder.id, props.folder.displayName || props.folder.name)
  }
}
</script>

<template>
  <div>
    <!-- 文件夹 item -->
    <Button
      type="button"
      variant="ghost"
      :class="[
        'group relative h-8 w-full justify-between rounded px-0 transition-colors',
        selectedFolder === folder.id ? 'bg-accent text-accent-foreground' : '',
      ]"
      :style="{ paddingLeft: `${depth * 12 + 8}px` }"
      @click="handleItemClick"
    >
      <!-- 左侧：图标 + 名称 -->
      <div class="flex min-w-0 items-center gap-2">
        <!-- 文件夹图标 -->
        <svg
          xmlns="http://www.w3.org/2000/svg"
          width="14"
          height="14"
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          stroke-width="2"
          stroke-linecap="round"
          stroke-linejoin="round"
          class="flex-shrink-0"
        >
          <path
            d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z"
          />
        </svg>

        <!-- 文件夹名称 -->
        <span class="truncate text-sm">
          {{ folder.displayName || folder.name }}
        </span>
      </div>

      <!-- 右侧：箭头 -->
      <div
        class="flex flex-shrink-0 items-center gap-2"
        v-if="folder.children && folder.children.length > 0"
      >
        <span
          class="-mr-1.5 flex-shrink-0 rounded p-1.5 hover:bg-accent/80"
          @click.stop="toggleExpand"
        >
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round"
            class="transition-transform"
            :class="{ 'rotate-90': isExpanded }"
          >
            <path d="m9 18 6-6-6-6" />
          </svg>
        </span>
      </div>
    </Button>

    <!-- 递归渲染子文件夹 -->
    <div v-if="isExpanded && folder.children && folder.children.length > 0" class="space-y-1">
      <FolderTreeItem
        v-for="child in folder.children"
        :key="child.id"
        :folder="child"
        :selected-folder="selectedFolder"
        :depth="depth + 1"
        @select="(folderId, folderName) => emit('select', folderId, folderName)"
      />
    </div>
  </div>
</template>
