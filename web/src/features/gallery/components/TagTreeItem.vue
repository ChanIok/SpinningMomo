<script setup lang="ts">
import { ref } from 'vue'
import { Button } from '@/components/ui/button'
import type { TagTreeNode } from '../types'

interface Props {
  tag: TagTreeNode
  selectedTag: number | null
  depth?: number
}

const props = withDefaults(defineProps<Props>(), {
  depth: 0,
})

const emit = defineEmits<{
  select: [tagId: number, tagName: string]
}>()

// 展开状态
const isExpanded = ref(false)

// 切换展开状态（独立点击箭头）
function toggleExpand() {
  isExpanded.value = !isExpanded.value
}

// 处理 item 点击
function handleItemClick() {
  const isCurrentlySelected = props.selectedTag === props.tag.id
  const hasChildren = props.tag.children && props.tag.children.length > 0

  if (isCurrentlySelected && hasChildren) {
    // 已选中 + 有子项 → 切换展开
    isExpanded.value = !isExpanded.value
  } else {
    // 未选中 → 选中
    emit('select', props.tag.id, props.tag.name)
  }
}
</script>

<template>
  <div>
    <!-- 标签 item -->
    <Button
      type="button"
      variant="ghost"
      :class="[
        'group relative h-8 w-full justify-between rounded px-2 transition-colors',
        selectedTag === tag.id ? 'bg-accent text-accent-foreground' : '',
      ]"
      :style="{ paddingLeft: `${depth * 12 + 8}px` }"
      @click="handleItemClick"
    >
      <!-- 左侧：图标 + 名称 -->
      <div class="flex min-w-0 items-center gap-2">
        <!-- 标签图标 -->
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
            d="M12 2H2v10l9.29 9.29c.94.94 2.48.94 3.42 0l6.58-6.58c.94-.94.94-2.48 0-3.42L12 2Z"
          />
          <path d="M7 7h.01" />
        </svg>

        <!-- 标签名称 -->
        <span class="truncate text-sm">
          {{ tag.name }}
        </span>
      </div>

      <!-- 右侧：资产数量 + 箭头 -->
      <div class="flex flex-shrink-0 items-center gap-2">
        <!-- 资产数量 -->
        <span class="rounded border px-1.5 py-0.5 text-xs">{{ tag.assetCount }}</span>

        <!-- 展开/收起箭头 -->
        <span
          v-if="tag.children && tag.children.length > 0"
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
        <div v-else class="w-3 flex-shrink-0" />
      </div>
    </Button>

    <!-- 递归渲染子标签 -->
    <div v-if="isExpanded && tag.children && tag.children.length > 0" class="space-y-1">
      <TagTreeItem
        v-for="child in tag.children"
        :key="child.id"
        :tag="child"
        :selected-tag="selectedTag"
        :depth="depth + 1"
        @select="(tagId, tagName) => emit('select', tagId, tagName)"
      />
    </div>
  </div>
</template>
