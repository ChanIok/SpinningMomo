<script setup lang="ts">
import { computed, ref } from 'vue'
import { FolderOpen, Pen, RefreshCw, Sparkles, Trash2 } from 'lucide-vue-next'
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
import { useSettingsStore } from '@/features/settings/store'
import TagInlineEditor from '../tags/TagInlineEditor.vue'
import { useGalleryStore } from '../../store'
import type { FolderTreeNode } from '../../types'
import {
  hasGalleryAssetDragIds,
  readGalleryAssetDragIds,
} from '../../composables/useGalleryDragPayload'

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
  clearSelection: []
  renameDisplayName: [folderId: number, displayName: string]
  openInExplorer: [folderId: number]
  removeWatch: [folderId: number]
  rescanFolder: [folderId: number, folderName: string]
  extractInfinityNikkiMetadata: [folderId: number, folderName: string]
  dropAssetsToFolder: [folderId: number, assetIds: number[]]
}>()

const { t } = useI18n()

const galleryStore = useGalleryStore()
const settingsStore = useSettingsStore()
const infinityNikkiEnabled = computed(
  () => settingsStore.appSettings.extensions.infinityNikki.enable
)

// 展开状态放在 gallery store，递归节点重挂载后也能恢复，并可跨会话持久化。
const isExpanded = computed(() => galleryStore.isFolderExpanded(props.folder.id))
const isEditingDisplayName = ref(false)
const showRemoveDialog = ref(false)
const shouldPreventAutoFocus = ref(false)
const isDragOver = ref(false)

const isRootFolder = computed(
  () => props.folder.parentId === undefined || props.folder.parentId === null
)

// 切换展开状态（独立点击箭头）
function toggleExpand() {
  galleryStore.toggleFolderExpanded(props.folder.id)
}

// 处理 item 点击
function handleItemClick() {
  const isCurrentlySelected = props.selectedFolder === props.folder.id
  const hasChildren = props.folder.children && props.folder.children.length > 0

  if (isCurrentlySelected && hasChildren) {
    // 已选中 + 有子项 → 切换展开
    galleryStore.toggleFolderExpanded(props.folder.id)
  } else if (isCurrentlySelected) {
    // 已选中 + 无子项 → 取消选择，回到未筛选状态
    emit('clearSelection')
  } else {
    // 未选中 → 选中
    emit('select', props.folder.id, props.folder.displayName || props.folder.name)
  }
}

function startRenameDisplayName() {
  isEditingDisplayName.value = true
  shouldPreventAutoFocus.value = true
}

function handleRenameConfirm(newName: string) {
  emit('renameDisplayName', props.folder.id, newName)
  isEditingDisplayName.value = false
}

function handleRenameCancel() {
  isEditingDisplayName.value = false
}

function handleOpenInExplorer() {
  emit('openInExplorer', props.folder.id)
}

function handleExtractInfinityNikkiMetadata() {
  emit(
    'extractInfinityNikkiMetadata',
    props.folder.id,
    props.folder.displayName || props.folder.name
  )
}

function handleRescanFolder() {
  emit('rescanFolder', props.folder.id, props.folder.displayName || props.folder.name)
}

function requestRemoveWatch() {
  showRemoveDialog.value = true
}

function confirmRemoveWatch() {
  emit('removeWatch', props.folder.id)
  showRemoveDialog.value = false
}

function handleContextMenuCloseAutoFocus(event: Event) {
  if (shouldPreventAutoFocus.value) {
    event.preventDefault()
    shouldPreventAutoFocus.value = false
  }
}

function handleDragEnter(event: DragEvent) {
  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  event.preventDefault()
  // 标记可放置态，用于侧边栏节点高亮反馈。
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
  emit('dropAssetsToFolder', props.folder.id, assetIds)
}
</script>

<template>
  <div>
    <AlertDialog v-model:open="showRemoveDialog">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>
            {{
              t('gallery.sidebar.folders.removeWatch.confirmTitle', {
                name: folder.displayName || folder.name,
              })
            }}
          </AlertDialogTitle>
          <AlertDialogDescription>
            {{ t('gallery.sidebar.folders.removeWatch.confirmDescription') }}
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>{{
            t('gallery.sidebar.folders.removeWatch.cancel')
          }}</AlertDialogCancel>
          <AlertDialogAction @click="confirmRemoveWatch">
            {{ t('gallery.sidebar.folders.removeWatch.confirm') }}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>

    <div v-if="isEditingDisplayName" class="px-2" :style="{ paddingLeft: `${depth * 12}px` }">
      <TagInlineEditor
        :initial-value="folder.displayName || folder.name"
        :placeholder="t('gallery.sidebar.folders.rename.placeholder')"
        @confirm="handleRenameConfirm"
        @cancel="handleRenameCancel"
      />
    </div>

    <ContextMenu v-else>
      <ContextMenuTrigger as-child>
        <button
          type="button"
          :class="
            cn(
              'group relative flex h-8 w-full cursor-pointer items-center justify-between rounded-md border-0 bg-transparent px-0 text-left text-sm transition-colors duration-200 ease-out outline-none',
              'focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2',
              isDragOver ? 'bg-primary/12 text-primary' : '',
              selectedFolder === folder.id
                ? 'bg-sidebar-accent font-medium text-primary hover:text-primary [&_svg]:text-primary'
                : 'text-sidebar-foreground hover:bg-sidebar-hover hover:text-sidebar-accent-foreground'
            )
          "
          :style="{ paddingLeft: `${depth * 12 + 8}px` }"
          @click="handleItemClick"
          @dragenter="handleDragEnter"
          @dragover="handleDragOver"
          @dragleave="handleDragLeave"
          @drop="handleDrop"
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
            <span class="font-tnum truncate text-sm">
              {{ folder.displayName || folder.name }}
            </span>
          </div>

          <!-- 右侧：箭头 -->
          <div
            class="flex flex-shrink-0 items-center gap-2"
            v-if="folder.children && folder.children.length > 0"
          >
            <span
              class="-mr-0.5 flex-shrink-0 rounded-md p-1.5 hover:bg-sidebar-hover"
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
        </button>
      </ContextMenuTrigger>

      <ContextMenuContent @close-auto-focus="handleContextMenuCloseAutoFocus">
        <ContextMenuItem @click="startRenameDisplayName">
          <Pen />
          {{ t('gallery.sidebar.folders.menu.renameDisplayName') }}
        </ContextMenuItem>
        <ContextMenuItem @click="handleOpenInExplorer">
          <FolderOpen />
          {{ t('gallery.sidebar.folders.menu.openInExplorer') }}
        </ContextMenuItem>
        <ContextMenuItem @click="handleRescanFolder">
          <RefreshCw />
          {{ t('gallery.sidebar.folders.menu.rescan') }}
        </ContextMenuItem>
        <ContextMenuItem v-if="infinityNikkiEnabled" @click="handleExtractInfinityNikkiMetadata">
          <Sparkles />
          {{ t('gallery.sidebar.folders.menu.extractInfinityNikkiMetadata') }}
        </ContextMenuItem>
        <template v-if="isRootFolder">
          <ContextMenuSeparator />
          <ContextMenuItem variant="destructive" @click="requestRemoveWatch">
            <Trash2 />
            {{ t('gallery.sidebar.folders.menu.removeWatch') }}
          </ContextMenuItem>
        </template>
      </ContextMenuContent>
    </ContextMenu>

    <!-- 递归渲染子文件夹 -->
    <div v-if="isExpanded && folder.children && folder.children.length > 0" class="space-y-1">
      <FolderTreeItem
        v-for="child in folder.children"
        :key="child.id"
        :folder="child"
        :selected-folder="selectedFolder"
        :depth="depth + 1"
        @select="(folderId, folderName) => emit('select', folderId, folderName)"
        @clear-selection="() => emit('clearSelection')"
        @rename-display-name="
          (folderId, displayName) => emit('renameDisplayName', folderId, displayName)
        "
        @open-in-explorer="(folderId) => emit('openInExplorer', folderId)"
        @remove-watch="(folderId) => emit('removeWatch', folderId)"
        @rescan-folder="(folderId, folderName) => emit('rescanFolder', folderId, folderName)"
        @extract-infinity-nikki-metadata="
          (folderId, folderName) => emit('extractInfinityNikkiMetadata', folderId, folderName)
        "
        @drop-assets-to-folder="
          (folderId, assetIds) => emit('dropAssetsToFolder', folderId, assetIds)
        "
      />
    </div>
  </div>
</template>
