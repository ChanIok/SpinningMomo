<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { cn } from '@/lib/utils'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { Plus } from 'lucide-vue-next'
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
import { useGallerySidebar, useGalleryData } from '../../composables'
import { useGalleryStore } from '../../store'
import type { FolderTreeNode } from '../../types'
import FolderTreeItem from '../folders/FolderTreeItem.vue'
import TagTreeItem from '../tags/TagTreeItem.vue'
import TagInlineEditor from '../tags/TagInlineEditor.vue'
import GalleryScanDialog from '../dialogs/GalleryScanDialog.vue'
import InfinityNikkiMetadataExtractDialog from '../infinity_nikki/InfinityNikkiMetadataExtractDialog.vue'
import { galleryApi } from '../../api'
import { hasGalleryAssetDragIds } from '../../composables/useGalleryDragPayload'
import type { ScanAssetsParams } from '../../api/dto'
import {
  extractInfinityNikkiUidFromFolderPath,
  INFINITY_NIKKI_LAST_UID_STORAGE_KEY,
} from '@/extensions/infinity_nikki'

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
const infinityNikkiMetadataInitialUid = ref('')
const showRescanDialog = ref(false)
const rescanFolderId = ref<number | null>(null)
const rescanFolderName = ref('')

// 区块标题表示该维度的「全体 / 未限定」：与 store.filter 一致，不跟详情面板耦合
const isFolderTitleSelected = computed(() => selectedFolder.value === null)
const isTagTitleSelected = computed(() => selectedTag.value === null)

function startAddFolder() {
  showAddFolderDialog.value = true
}

function handleAddFolderDialogOpenChange(open: boolean) {
  showAddFolderDialog.value = open
}

function openInfinityNikkiMetadataDialog(folderId: number, folderName: string) {
  infinityNikkiMetadataFolderId.value = folderId
  infinityNikkiMetadataFolderName.value = folderName
  const targetFolder = findFolderById(folders.value, folderId)
  const uidFromFolderPath = targetFolder
    ? extractInfinityNikkiUidFromFolderPath(targetFolder.path)
    : null
  const uidFromStorage = localStorage.getItem(INFINITY_NIKKI_LAST_UID_STORAGE_KEY)?.trim() || ''
  infinityNikkiMetadataInitialUid.value = uidFromFolderPath ?? uidFromStorage
  showInfinityNikkiMetadataDialog.value = true
}

function handleInfinityNikkiMetadataDialogOpenChange(open: boolean) {
  showInfinityNikkiMetadataDialog.value = open
  if (!open) {
    infinityNikkiMetadataFolderId.value = null
    infinityNikkiMetadataFolderName.value = ''
    infinityNikkiMetadataInitialUid.value = ''
  }
}

function openRescanDialog(folderId: number, folderName: string) {
  rescanFolderId.value = folderId
  rescanFolderName.value = folderName
  showRescanDialog.value = true
}

function handleRescanDialogOpenChange(open: boolean) {
  showRescanDialog.value = open
}

function resetRescanDialogState() {
  showRescanDialog.value = false
  rescanFolderId.value = null
  rescanFolderName.value = ''
}

function findFolderById(nodes: FolderTreeNode[], folderId: number): FolderTreeNode | null {
  for (const node of nodes) {
    if (node.id === folderId) {
      return node
    }
    const foundInChildren = findFolderById(node.children, folderId)
    if (foundInChildren) {
      return foundInChildren
    }
  }
  return null
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

async function confirmRescanFolder() {
  const folderId = rescanFolderId.value
  if (folderId === null) {
    return
  }

  const targetFolder = findFolderById(folders.value, folderId)
  if (!targetFolder) {
    toast.error(t('gallery.sidebar.folders.rescan.failedTitle'), {
      description: t('gallery.sidebar.folders.rescan.folderNotFoundDescription'),
    })
    resetRescanDialogState()
    return
  }

  try {
    const rescanParams: ScanAssetsParams = {
      directory: targetFolder.path,
      forceReanalyze: true,
      rebuildThumbnails: true,
    }
    const result = await galleryData.startScanAssets(rescanParams)
    toast.success(t('gallery.sidebar.folders.rescan.queuedTitle'), {
      description: t('gallery.sidebar.folders.rescan.queuedDescription', {
        taskId: result.taskId,
      }),
    })
    resetRescanDialogState()
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.rescan.failedTitle'), { description: message })
  }
}

async function handleDropAssetsToFolder(folderId: number, assetIds: number[]) {
  const uniqueIds = [...new Set(assetIds)]
  if (uniqueIds.length === 0) {
    return
  }

  try {
    const result = await galleryApi.moveAssetsToFolder({
      ids: uniqueIds,
      targetFolderId: folderId,
    })
    const affectedCount = result.affectedCount ?? 0
    const failedCount = result.failedCount ?? 0
    const notFoundCount = result.notFoundCount ?? 0
    const unchangedCount = result.unchangedCount ?? 0
    if (!result.success && affectedCount === 0) {
      throw new Error(
        t('gallery.sidebar.folders.moveAssets.failedDescription', {
          failed: failedCount,
          notFound: notFoundCount,
          unchanged: unchangedCount,
        })
      )
    }

    await Promise.all([
      galleryData.loadFolderTree({ silent: true }),
      galleryData.refreshCurrentQuery(),
    ])
    galleryStore.clearSelection()

    if (result.success) {
      toast.success(t('gallery.sidebar.folders.moveAssets.successTitle'), {
        description: t('gallery.sidebar.folders.moveAssets.successDescription', {
          count: affectedCount,
        }),
      })
    } else {
      toast.warning(t('gallery.sidebar.folders.moveAssets.partialTitle'), {
        description: t('gallery.sidebar.folders.moveAssets.partialDescription', {
          moved: affectedCount,
          failed: failedCount,
          notFound: notFoundCount,
          unchanged: unchangedCount,
        }),
      })
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.moveAssets.failedTitle'), { description: message })
  }
}

async function handleDropAssetsToTag(tagId: number, assetIds: number[]) {
  const uniqueIds = [...new Set(assetIds)]
  if (uniqueIds.length === 0) {
    return
  }

  try {
    const result = await galleryApi.addTagToAssets({
      assetIds: uniqueIds,
      tagId,
    })
    const affectedCount = result.affectedCount ?? 0
    const failedCount = result.failedCount ?? 0
    const unchangedCount = result.unchangedCount ?? 0

    if (!result.success && affectedCount === 0) {
      throw new Error(
        t('gallery.sidebar.tags.addAssets.failedDescription', {
          failed: failedCount,
          unchanged: unchangedCount,
        })
      )
    }

    await Promise.all([loadTagTree(), galleryData.refreshCurrentQuery()])
    galleryStore.clearSelection()

    if (result.success && failedCount === 0 && unchangedCount === 0) {
      toast.success(t('gallery.sidebar.tags.addAssets.successTitle'), {
        description: t('gallery.sidebar.tags.addAssets.successDescription', {
          count: affectedCount,
        }),
      })
    } else {
      toast.warning(t('gallery.sidebar.tags.addAssets.partialTitle'), {
        description: t('gallery.sidebar.tags.addAssets.partialDescription', {
          affected: affectedCount,
          failed: failedCount,
          unchanged: unchangedCount,
        }),
      })
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.tags.addAssets.failedTitle'), { description: message })
  }
}

function handleSidebarDragOver(event: DragEvent) {
  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  // 让侧边栏空白区域也保持可放置手势，避免出现系统“禁止”图标。
  event.preventDefault()
  if (event.dataTransfer) {
    event.dataTransfer.dropEffect = 'move'
  }
}

function handleSidebarDrop(event: DragEvent) {
  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  // 真正移动仍由 FolderTreeItem 的 drop 处理；这里仅消费默认 drop 行为。
  event.preventDefault()
}

onMounted(() => {
  galleryData.loadFolderTree()
  loadTagTree()
})
</script>

<template>
  <div class="flex h-full flex-col" @dragover="handleSidebarDragOver" @drop="handleSidebarDrop">
    <AlertDialog :open="showRescanDialog" @update:open="handleRescanDialogOpenChange">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>
            {{
              t('gallery.sidebar.folders.rescan.confirmTitle', {
                name: rescanFolderName,
              })
            }}
          </AlertDialogTitle>
          <AlertDialogDescription>
            {{ t('gallery.sidebar.folders.rescan.confirmDescription') }}
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel @click="resetRescanDialogState">
            {{ t('gallery.sidebar.folders.rescan.cancel') }}
          </AlertDialogCancel>
          <AlertDialogAction @click="confirmRescanFolder">
            {{ t('gallery.sidebar.folders.rescan.confirm') }}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>

    <!-- 导航菜单 -->
    <div class="flex-1 overflow-auto p-4">
      <!-- 文件夹区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <button
            type="button"
            :class="
              cn(
                'cursor-pointer rounded-md px-2 py-1 text-left text-xs font-medium tracking-wider uppercase transition-colors duration-200 ease-out',
                'focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2 focus-visible:outline-none',
                isFolderTitleSelected
                  ? 'bg-sidebar-accent font-medium text-primary hover:text-primary'
                  : 'text-sidebar-foreground hover:bg-sidebar-accent hover:text-sidebar-accent-foreground'
              )
            "
            @click="clearFolderFilter"
          >
            {{ t('gallery.sidebar.folders.title') }}
          </button>
          <Button variant="sidebarGhost" size="icon-xs" @click="startAddFolder">
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
            @clear-selection="clearFolderFilter"
            @rename-display-name="handleRenameFolderDisplayName"
            @open-in-explorer="handleOpenFolderInExplorer"
            @remove-watch="handleRemoveFolderWatch"
            @rescan-folder="openRescanDialog"
            @extract-infinity-nikki-metadata="openInfinityNikkiMetadataDialog"
            @drop-assets-to-folder="handleDropAssetsToFolder"
          />
        </div>
      </div>

      <Separator class="my-4" />

      <!-- 标签区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <button
            type="button"
            :class="
              cn(
                'cursor-pointer rounded-md px-2 py-1 text-left text-xs font-medium tracking-wider uppercase transition-colors duration-200 ease-out',
                'focus-visible:ring-2 focus-visible:ring-sidebar-ring focus-visible:ring-offset-2 focus-visible:outline-none',
                isTagTitleSelected
                  ? 'bg-sidebar-accent font-medium text-primary hover:text-primary'
                  : 'text-sidebar-foreground hover:bg-sidebar-accent hover:text-sidebar-accent-foreground'
              )
            "
            @click="clearTagFilter"
          >
            {{ t('gallery.sidebar.tags.title') }}
          </button>
          <Button variant="sidebarGhost" size="icon-xs" @click="startCreateTag">
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
            @drop-assets-to-tag="handleDropAssetsToTag"
          />
        </div>
      </div>
    </div>

    <GalleryScanDialog :open="showAddFolderDialog" @update:open="handleAddFolderDialogOpenChange" />
    <InfinityNikkiMetadataExtractDialog
      :open="showInfinityNikkiMetadataDialog"
      :folder-id="infinityNikkiMetadataFolderId"
      :folder-name="infinityNikkiMetadataFolderName"
      :initial-uid="infinityNikkiMetadataInitialUid"
      @update:open="handleInfinityNikkiMetadataDialogOpenChange"
    />
  </div>
</template>
