<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { Plus } from 'lucide-vue-next'
import { useGallerySidebar, useGalleryData } from '../composables'
import { useGalleryStore } from '../store'
import type { FolderTreeNode } from '../types'
import FolderTreeItem from '../components/FolderTreeItem.vue'
import TagTreeItem from '../components/TagTreeItem.vue'
import TagInlineEditor from '../components/TagInlineEditor.vue'
import GalleryScanDialog from '../components/GalleryScanDialog.vue'
import InfinityNikkiMetadataExtractDialog from '../components/InfinityNikkiMetadataExtractDialog.vue'

const galleryData = useGalleryData()
const galleryStore = useGalleryStore()
const { toast } = useToast()
const { t } = useI18n()

const {
  folders,
  foldersLoading,
  foldersError,
  tags,
  tagsLoading,
  tagsError,
  selectedFolder,
  selectedTag,
  selectFolder,
  clearFolderFilter,
  updateFolderDisplayName,
  openFolderInExplorer,
  removeFolderWatch,
  clearTagFilter,
  selectTag,
  loadTagTree,
  createTag,
  updateTag,
  deleteTag,
} = useGallerySidebar()

// 标签创建状态
const isCreatingTag = ref(false)

const showAddFolderDialog = ref(false)
const showInfinityNikkiMetadataDialog = ref(false)
const infinityNikkiMetadataFolderId = ref<number | null>(null)
const infinityNikkiMetadataFolderName = ref('')

const isFolderTitleSelected = computed(() => {
  return galleryStore.sidebar.activeSection === 'folders' && selectedFolder.value === null
})

const isTagTitleSelected = computed(() => {
  return galleryStore.sidebar.activeSection === 'tags' && selectedTag.value === null
})

function startAddFolder() {
  showAddFolderDialog.value = true
}

function handleAddFolderDialogOpenChange(open: boolean) {
  showAddFolderDialog.value = open
}

function openInfinityNikkiMetadataDialog(folderId: number, folderName: string) {
  infinityNikkiMetadataFolderId.value = folderId
  infinityNikkiMetadataFolderName.value = folderName
  showInfinityNikkiMetadataDialog.value = true
}

function handleInfinityNikkiMetadataDialogOpenChange(open: boolean) {
  showInfinityNikkiMetadataDialog.value = open
  if (!open) {
    infinityNikkiMetadataFolderId.value = null
    infinityNikkiMetadataFolderName.value = ''
  }
}

function startCreateTag() {
  isCreatingTag.value = true
}

async function handleCreateTag(name: string) {
  try {
    await createTag(name)
    isCreatingTag.value = false
  } catch (error) {
    console.error('Failed to create tag:', error)
  }
}

function handleCancelCreateTag() {
  isCreatingTag.value = false
}

async function handleRenameTag(tagId: number, newName: string) {
  try {
    await updateTag(tagId, newName)
  } catch (error) {
    console.error('Failed to rename tag:', error)
  }
}

async function handleCreateChildTag(parentId: number, name: string) {
  try {
    await createTag(name, parentId)
  } catch (error) {
    console.error('Failed to create child tag:', error)
  }
}

async function handleDeleteTag(tagId: number) {
  try {
    await deleteTag(tagId)
  } catch (error) {
    console.error('Failed to delete tag:', error)
  }
}

function folderExistsById(nodes: FolderTreeNode[], folderId: number): boolean {
  for (const node of nodes) {
    if (node.id === folderId) {
      return true
    }
    if (node.children && folderExistsById(node.children, folderId)) {
      return true
    }
  }
  return false
}

async function handleRenameFolderDisplayName(folderId: number, displayName: string) {
  try {
    await updateFolderDisplayName(folderId, displayName)
    await galleryData.loadFolderTree()

    if (selectedFolder.value === folderId) {
      const folderName = displayName.trim()
      selectFolder(folderId, folderName)
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.rename.failedTitle'), { description: message })
  }
}

async function handleOpenFolderInExplorer(folderId: number) {
  try {
    await openFolderInExplorer(folderId)
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.openInExplorer.failedTitle'), { description: message })
  }
}

async function handleRemoveFolderWatch(folderId: number) {
  try {
    await removeFolderWatch(folderId)

    await galleryData.loadFolderTree()

    const currentSelectedFolderId = selectedFolder.value
    if (
      currentSelectedFolderId !== null &&
      !folderExistsById(galleryStore.folders, currentSelectedFolderId)
    ) {
      clearFolderFilter()
    }

    if (galleryStore.isTimelineMode) {
      await galleryData.loadTimelineData()
    } else {
      await galleryData.loadAllAssets()
    }

    toast.success(t('gallery.sidebar.folders.removeWatch.successTitle'), {
      description: t('gallery.sidebar.folders.removeWatch.successDescription'),
    })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.removeWatch.failedTitle'), { description: message })
  }
}

onMounted(() => {
  galleryData.loadFolderTree()
  loadTagTree()
})
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 导航菜单 -->
    <div class="flex-1 overflow-auto p-4">
      <!-- 文件夹区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <button
            type="button"
            :class="[
              'cursor-pointer rounded px-2 py-1 text-xs font-medium tracking-wider uppercase transition-colors',
              isFolderTitleSelected
                ? 'bg-accent text-accent-foreground'
                : 'text-muted-foreground hover:text-foreground',
            ]"
            @click="clearFolderFilter"
          >
            {{ t('gallery.sidebar.folders.title') }}
          </button>
          <Button variant="ghost" size="icon" class="h-6 w-6" @click="startAddFolder">
            <Plus class="h-3 w-3" />
          </Button>
        </div>
        <!-- 加载状态 -->
        <div v-if="foldersLoading" class="px-2 text-xs text-muted-foreground">
          {{ t('gallery.sidebar.common.loading') }}
        </div>
        <!-- 错误状态 -->
        <div v-else-if="foldersError" class="px-2 text-xs text-destructive">
          {{ foldersError }}
        </div>
        <!-- 文件夹树 -->
        <div v-else class="space-y-1">
          <FolderTreeItem
            v-for="folder in folders"
            :key="folder.id"
            :folder="folder"
            :selected-folder="selectedFolder"
            :depth="0"
            @select="selectFolder"
            @rename-display-name="handleRenameFolderDisplayName"
            @open-in-explorer="handleOpenFolderInExplorer"
            @remove-watch="handleRemoveFolderWatch"
            @extract-infinity-nikki-metadata="openInfinityNikkiMetadataDialog"
          />
        </div>
      </div>

      <Separator class="my-4" />

      <!-- 标签区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <button
            type="button"
            :class="[
              'cursor-pointer rounded px-2 py-1 text-xs font-medium tracking-wider uppercase transition-colors',
              isTagTitleSelected
                ? 'bg-accent text-accent-foreground'
                : 'text-muted-foreground hover:text-foreground',
            ]"
            @click="clearTagFilter"
          >
            {{ t('gallery.sidebar.tags.title') }}
          </button>
          <Button variant="ghost" size="icon" class="h-6 w-6" @click="startCreateTag">
            <Plus class="h-3 w-3" />
          </Button>
        </div>
        <!-- 加载状态 -->
        <div v-if="tagsLoading" class="px-2 text-xs text-muted-foreground">
          {{ t('gallery.sidebar.common.loading') }}
        </div>
        <!-- 错误状态 -->
        <div v-else-if="tagsError" class="px-2 text-xs text-destructive">
          {{ tagsError }}
        </div>
        <!-- 标签树 -->
        <div v-else class="space-y-1">
          <!-- 快速创建标签 -->
          <div v-if="isCreatingTag" class="px-2">
            <TagInlineEditor
              :placeholder="t('gallery.sidebar.tags.createPlaceholder')"
              @confirm="handleCreateTag"
              @cancel="handleCancelCreateTag"
            />
          </div>
          <!-- 标签列表 -->
          <TagTreeItem
            v-for="tag in tags"
            :key="tag.id"
            :tag="tag"
            :selected-tag="selectedTag"
            :depth="0"
            @select="selectTag"
            @rename="handleRenameTag"
            @create-child="handleCreateChildTag"
            @delete="handleDeleteTag"
          />
        </div>
      </div>
    </div>

    <GalleryScanDialog :open="showAddFolderDialog" @update:open="handleAddFolderDialogOpenChange" />
    <InfinityNikkiMetadataExtractDialog
      :open="showInfinityNikkiMetadataDialog"
      :folder-id="infinityNikkiMetadataFolderId"
      :folder-name="infinityNikkiMetadataFolderName"
      @update:open="handleInfinityNikkiMetadataDialogOpenChange"
    />
  </div>
</template>
