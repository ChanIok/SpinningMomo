<script setup lang="ts">
import { computed, ref } from 'vue'
import { ChevronRight, Pen, Plus, Tag, Trash2 } from '@lucide/vue'
import { cn } from '@/lib/utils'
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
import { useI18n } from '@/composables/useI18n'
import TagInlineEditor from './TagInlineEditor.vue'
import { useGalleryStore } from '../../store'
import type { TagTreeNode } from '../../types'
import {
  hasGalleryAssetDragIds,
  readGalleryAssetDragIds,
} from '../../composables/useGalleryDragPayload'

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
  dropAssetsToTag: [tagId: number, assetIds: number[]]
}>()

const { t } = useI18n()
const galleryStore = useGalleryStore()
// 与文件夹树保持一致：展开状态统一走 store，而不是递归组件各自记一份局部状态。
const isExpanded = computed(() => galleryStore.isTagExpanded(props.tag.id))

// 编辑状态
const isEditing = ref(false)
const isCreatingChild = ref(false)

// 删除确认对话框状态
const showDeleteDialog = ref(false)
const isDragOver = ref(false)

// 控制是否阻止 ContextMenu 的 closeAutoFocus
const shouldPreventAutoFocus = ref(false)

// 切换展开状态（独立点击箭头）
function toggleExpand() {
  galleryStore.toggleTagExpanded(props.tag.id)
}

// 处理 item 点击
function handleItemClick() {
  if (isEditing.value) return

  const isCurrentlySelected = props.selectedTag === props.tag.id
  const hasChildren = props.tag.children && props.tag.children.length > 0

  if (isCurrentlySelected) {
    if (hasChildren) {
      // 已选中 + 有子项 → 切换展开/折叠
      galleryStore.toggleTagExpanded(props.tag.id)
    }
    // 已选中 + 无子项 → 保持选中状态
    return
  }

  // 未选中 → 选中该标签
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
  galleryStore.setTagExpanded(props.tag.id, true)
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

function handleDragEnter(event: DragEvent) {
  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  event.preventDefault()
  isDragOver.value = true
}

function handleDragOver(event: DragEvent) {
  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  event.preventDefault()
  if (event.dataTransfer) {
    event.dataTransfer.dropEffect = 'move'
  }
  isDragOver.value = true
}

function handleDragLeave() {
  isDragOver.value = false
}

function handleDrop(event: DragEvent) {
  event.preventDefault()
  isDragOver.value = false
  const assetIds = readGalleryAssetDragIds(event)
  if (assetIds.length === 0) {
    return
  }
  emit('dropAssetsToTag', props.tag.id, assetIds)
}
</script>

<template>
  <div>
    <!-- 删除确认对话框 -->
    <AlertDialog v-model:open="showDeleteDialog">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>
            {{ t('gallery.sidebar.tags.delete.confirmTitle') }}
          </AlertDialogTitle>
          <AlertDialogDescription>
            {{ t('gallery.sidebar.tags.delete.confirmDescription', { name: tag.name }) }}
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>
            {{ t('gallery.sidebar.tags.delete.cancel') }}
          </AlertDialogCancel>
          <AlertDialogAction @click="confirmDelete">
            {{ t('gallery.sidebar.tags.delete.confirm') }}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>

    <!-- 标签 item -->
    <div v-if="isEditing">
      <TagInlineEditor
        :initial-value="tag.name"
        :depth="depth"
        @confirm="handleRenameConfirm"
        @cancel="handleRenameCancel"
      />
    </div>
    <!-- 右键菜单 -->
    <ContextMenu v-else>
      <ContextMenuTrigger as-child>
        <button
          type="button"
          :class="
            cn(
              'group relative flex h-8 w-full cursor-default items-center justify-between rounded-md border-0 bg-transparent px-0 text-left text-sm transition-colors duration-200 ease-out outline-none',
              'focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2',
              isDragOver ? 'bg-primary/12 text-primary' : '',
              selectedTag === tag.id
                ? 'bg-sidebar-accent font-medium text-primary hover:text-primary [&_svg]:text-primary'
                : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
            )
          "
          :style="{ paddingLeft: `${depth * 12 + 8}px` }"
          @click="handleItemClick"
          @dblclick="handleDoubleClick"
          @dragenter="handleDragEnter"
          @dragover="handleDragOver"
          @dragleave="handleDragLeave"
          @drop="handleDrop"
        >
          <!-- 左侧：图标 + 名称 -->
          <div class="flex min-w-0 items-center gap-2">
            <!-- 标签图标 -->
            <Tag class="h-3.5 w-3.5 flex-shrink-0" />

            <!-- 标签名称 -->
            <span class="truncate text-sm">
              {{ tag.name }}
            </span>
          </div>

          <!-- 右侧：展开箭头 -->
          <div
            class="flex flex-shrink-0 items-center gap-2"
            v-if="tag.children && tag.children.length > 0"
          >
            <!-- 展开/收起箭头 -->
            <span
              class="mr-1 flex-shrink-0 rounded-sm p-1 hover:bg-sidebar-hover"
              @click.stop="toggleExpand"
            >
              <ChevronRight
                class="h-4 w-4 transition-transform"
                :class="{ 'rotate-90': isExpanded }"
              />
            </span>
          </div>
        </button>
      </ContextMenuTrigger>

      <ContextMenuContent @close-auto-focus="handleContextMenuCloseAutoFocus">
        <ContextMenuItem @click="startRename">
          <Pen />
          {{ t('gallery.sidebar.tags.menu.rename') }}
        </ContextMenuItem>
        <ContextMenuItem @click="startCreateChild">
          <Plus />
          {{ t('gallery.sidebar.tags.menu.createChild') }}
        </ContextMenuItem>
        <ContextMenuSeparator />
        <ContextMenuItem variant="destructive" @click="requestDelete">
          <Trash2 />
          {{ t('gallery.sidebar.tags.menu.delete') }}
        </ContextMenuItem>
      </ContextMenuContent>
    </ContextMenu>

    <!-- 递归渲染子标签 -->
    <div v-if="isExpanded" class="space-y-1">
      <!-- 创建子标签 -->
      <div v-if="isCreatingChild">
        <TagInlineEditor
          :depth="depth + 1"
          :placeholder="t('gallery.sidebar.tags.createPlaceholder')"
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
        @drop-assets-to-tag="(tagId, assetIds) => emit('dropAssetsToTag', tagId, assetIds)"
      />
    </div>
  </div>
</template>
