<script setup lang="ts">
import { ref } from 'vue'
import { Button } from '@/components/ui/button'
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuTrigger,
} from '@/components/ui/context-menu'
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from '@/components/ui/alert-dialog'
import TagInlineEditor from './TagInlineEditor.vue'
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
  rename: [tagId: number, newName: string]
  createChild: [parentId: number, name: string]
  delete: [tagId: number]
}>()

// 展开状态
const isExpanded = ref(false)

// 编辑状态
const isEditing = ref(false)
const isCreatingChild = ref(false)

// 删除确认对话框状态
const showDeleteDialog = ref(false)

// 控制是否阻止 ContextMenu 的 closeAutoFocus
const shouldPreventAutoFocus = ref(false)

// 切换展开状态（独立点击箭头）
function toggleExpand() {
  isExpanded.value = !isExpanded.value
}

// 处理 item 点击
function handleItemClick() {
  if (isEditing.value) return
  
  // 移除了选中状态下点击展开子标签的逻辑
  // 现在只会选中标签，不会自动展开
  emit('select', props.tag.id, props.tag.name)
}

// 双击重命名
function handleDoubleClick() {
  // isEditing.value = true
}

// 确认重命名
function handleRenameConfirm(newName: string) {
  emit('rename', props.tag.id, newName)
  isEditing.value = false
}

// 取消重命名
function handleRenameCancel() {
  isEditing.value = false
}

// 开始创建子标签
function startCreateChild() {
  isExpanded.value = true // 自动展开
  isCreatingChild.value = true
  shouldPreventAutoFocus.value = true // 阻止 ContextMenu 关闭时的自动聚焦
}

// 确认创建子标签
function handleCreateChildConfirm(name: string) {
  emit('createChild', props.tag.id, name)
  isCreatingChild.value = false
}

// 取消创建子标签
function handleCreateChildCancel() {
  isCreatingChild.value = false
}

// 处理 ContextMenu 关闭时的自动聚焦
function handleContextMenuCloseAutoFocus(event: Event) {
  if (shouldPreventAutoFocus.value) {
    event.preventDefault() // 阻止自动聚焦，让输入框保持焦点
    shouldPreventAutoFocus.value = false // 重置标志
  }
}

// 右键菜单操作
function startRename() {
  isEditing.value = true
  shouldPreventAutoFocus.value = true // 阻止 ContextMenu 关闭时的自动聚焦
}

function requestDelete() {
  showDeleteDialog.value = true
}

function confirmDelete() {
  emit('delete', props.tag.id)
  showDeleteDialog.value = false
}
</script>

<template>
  <div>
    <!-- 删除确认对话框 -->
    <AlertDialog v-model:open="showDeleteDialog">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>确认删除标签？</AlertDialogTitle>
          <AlertDialogDescription>
            将删除标签「{{ tag.name }}」及其所有关联。此操作不可恢复。
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>取消</AlertDialogCancel>
          <AlertDialogAction @click="confirmDelete">确认删除</AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>

    <!-- 标签 item -->
    <div
      v-if="isEditing"
      class="px-2"
      :style="{ paddingLeft: `${depth * 12}px` }"
    >
      <TagInlineEditor
        :initial-value="tag.name"
        placeholder="输入标签名..."
        @confirm="handleRenameConfirm"
        @cancel="handleRenameCancel"
      />
    </div>
    <!-- 右键菜单 -->
    <ContextMenu v-else>
      <ContextMenuTrigger as-child>
        <Button
          type="button"
          variant="ghost"
          :class="[
            'group relative h-8 w-full justify-between rounded px-2 transition-colors',
            selectedTag === tag.id ? 'bg-accent text-accent-foreground' : '',
          ]"
          :style="{ paddingLeft: `${depth * 12 + 8}px` }"
          @click="handleItemClick"
          @dblclick="handleDoubleClick"
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
      </ContextMenuTrigger>

      <ContextMenuContent @close-auto-focus="handleContextMenuCloseAutoFocus">
        <ContextMenuItem @click="startRename">
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
            class="mr-2"
          >
            <path d="M17 3a2.85 2.83 0 1 1 4 4L7.5 20.5 2 22l1.5-5.5Z" />
            <path d="m15 5 4 4" />
          </svg>
          重命名
        </ContextMenuItem>
        <ContextMenuItem @click="startCreateChild">
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
            class="mr-2"
          >
            <path d="M5 12h14" />
            <path d="M12 5v14" />
          </svg>
          添加子标签
        </ContextMenuItem>
        <ContextMenuSeparator />
        <ContextMenuItem @click="requestDelete" class="text-destructive focus:text-destructive">
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
            class="mr-2"
          >
            <path d="M3 6h18" />
            <path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6" />
            <path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2" />
          </svg>
          删除
        </ContextMenuItem>
      </ContextMenuContent>
    </ContextMenu>

    <!-- 递归渲染子标签 -->
    <div v-if="isExpanded" class="space-y-1">
      <!-- 创建子标签 -->
      <div v-if="isCreatingChild" class="px-2" :style="{ paddingLeft: `${(depth + 1) * 12}px` }">
        <TagInlineEditor
          placeholder="输入子标签名..."
          @confirm="handleCreateChildConfirm"
          @cancel="handleCreateChildCancel"
        />
      </div>
      <!-- 子标签列表 -->
      <TagTreeItem
        v-for="child in tag.children"
        :key="child.id"
        :tag="child"
        :selected-tag="selectedTag"
        :depth="depth + 1"
        @select="(tagId, tagName) => emit('select', tagId, tagName)"
        @rename="(tagId, newName) => emit('rename', tagId, newName)"
        @create-child="(parentId, name) => emit('createChild', parentId, name)"
        @delete="(tagId) => emit('delete', tagId)"
      />
    </div>
  </div>
</template>
