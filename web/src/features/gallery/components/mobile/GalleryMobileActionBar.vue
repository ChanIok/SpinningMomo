<script setup lang="ts">
import { computed, ref } from 'vue'
import {
  Copy,
  Download,
  ExternalLink,
  FolderOpen,
  LoaderCircle,
  MoreHorizontal,
  Star,
  Tag,
  Trash2,
  X,
} from '@lucide/vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { isLocalAccess } from '@/core/access'
import { Button } from '@/components/ui/button'
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
import { Sheet, SheetContent, SheetHeader, SheetTitle } from '@/components/ui/sheet'
import { useGalleryAssetActions, useGalleryData } from '../../composables'
import { useGalleryStore } from '../../store'
import { galleryApi } from '../../api'
import TagSelectorPopover from '../tags/TagSelectorPopover.vue'
import { isWebView } from '@/core/env'

const { t } = useI18n()
const { toast } = useToast()
const store = useGalleryStore()
const assetActions = useGalleryAssetActions()
const galleryData = useGalleryData()

const ratingSheetOpen = ref(false)
const tagSheetOpen = ref(false)
const moreSheetOpen = ref(false)
const tagIds = ref<number[]>([])
const tagLoading = ref(false)
const isDownloading = ref(false)
const downloadConfirmationOpen = ref(false)
const pendingDownloadIds = ref<number[]>([])

const LARGE_DOWNLOAD_CONFIRMATION_THRESHOLD = 100

const selectedCount = computed(() => store.selectedCount)
const selectedAssets = computed(() => {
  const selectedIdSet = store.selection.selectedIds
  const assets = [] as { id: number; reviewFlag: 'none' | 'picked' | 'rejected' }[]

  store.paginatedAssets.forEach((pageAssets) => {
    pageAssets.forEach((asset) => {
      if (selectedIdSet.has(asset.id)) {
        assets.push(asset)
      }
    })
  })

  return assets
})
const allSelectedRejected = computed(
  () =>
    selectedCount.value > 0 &&
    selectedAssets.value.length === selectedCount.value &&
    selectedAssets.value.every((asset) => asset.reviewFlag === 'rejected')
)
const canUseLocalFileSystem = computed(() => isLocalAccess())
const downloadAssetIds = computed(() => {
  if (assetActions.selectedAssetIds.value.length > 0) {
    return assetActions.selectedAssetIds.value
  }

  const activeAssetId = store.selection.activeAssetId
  return activeAssetId === undefined ? [] : [activeAssetId]
})

function closeActionSheet() {
  ratingSheetOpen.value = false
  tagSheetOpen.value = false
  moreSheetOpen.value = false
}

function handleRating(rating: number) {
  closeActionSheet()
  void (rating === 0
    ? assetActions.clearSelectedAssetsRating()
    : assetActions.setSelectedAssetsRating(rating))
}

function handleRejected() {
  if (allSelectedRejected.value) {
    void assetActions.clearSelectedAssetsRejected()
    return
  }

  void assetActions.setSelectedAssetsRejected()
}

async function openTagSheet() {
  tagSheetOpen.value = true
  tagLoading.value = true

  try {
    if (store.tags.length === 0) {
      await galleryData.loadTagTree()
    }

    const assetIds = assetActions.selectedAssetIds.value
    if (assetIds.length === 0) {
      tagIds.value = []
      return
    }

    if (assetIds.length === 1) {
      const tags = await galleryApi.getAssetTags(assetIds[0]!)
      tagIds.value = tags.map((tag) => tag.id)
      return
    }

    const summary = await galleryApi.getBatchSelectionSummary(assetIds)
    tagIds.value = summary.commonTags.map((tag) => tag.id)
  } catch (error) {
    console.error('Failed to load mobile gallery tags:', error)
    tagIds.value = []
  } finally {
    tagLoading.value = false
  }
}

async function handleTagToggle(tagId: number) {
  if (tagLoading.value) {
    return
  }

  tagLoading.value = true
  const hasTag = tagIds.value.includes(tagId)

  try {
    if (hasTag) {
      await assetActions.removeTagFromSelectedAssets(tagId)
      tagIds.value = tagIds.value.filter((id) => id !== tagId)
    } else {
      await assetActions.addTagToSelectedAssets(tagId)
      tagIds.value = [...tagIds.value, tagId]
    }
  } catch (error) {
    console.error('Failed to update mobile gallery tag:', error)
  } finally {
    tagLoading.value = false
  }
}

function handleOpenAssetDefault() {
  closeActionSheet()
  void assetActions.handleOpenAssetDefault()
}

function handleRevealAssetInExplorer() {
  closeActionSheet()
  void assetActions.handleRevealAssetInExplorer()
}

function handleCopyAssetsToClipboard() {
  closeActionSheet()
  void assetActions.handleCopyAssetsToClipboard()
}

function handleMoveToFolder() {
  closeActionSheet()
  assetActions.openMoveToFolderDialog()
}

function handleCopyTags() {
  closeActionSheet()
  void assetActions.copySelectedAssetTags()
}

function handlePasteTags() {
  closeActionSheet()
  void assetActions.pasteCopiedTagsToSelection()
}

function handleDelete() {
  closeActionSheet()
  void assetActions.requestDeleteAssets()
}

// 准备下载并触发浏览器附件请求；传入的 ID 已经是本次操作快照。
async function startDownload(assetIds: number[]) {
  if (isDownloading.value || assetIds.length === 0) {
    return
  }

  // 准备阶段可能包含 ZIP 创建，保持按钮忙碌到下载请求发出。
  isDownloading.value = true
  try {
    const result = await assetActions.prepareDownload(assetIds)
    if (!result) {
      return
    }

    // WebView 使用 HTTP 服务绝对地址，浏览器开发环境使用 Vite 代理相对地址。
    const downloadUrl = isWebView() ? result.localDownloadUrl : result.downloadUrl
    if (!downloadUrl) {
      throw new Error(t('gallery.mobile.download.unavailableDescription'))
    }

    // 让浏览器接管附件下载，不把媒体内容读进前端内存。
    const link = document.createElement('a')
    link.href = downloadUrl
    link.download = result.fileName
    link.rel = 'noopener'
    link.style.display = 'none'
    document.body.appendChild(link)
    link.click()
    link.remove()

    // 归档准备允许部分文件不可用，仍提示用户实际结果。
    if (result.failedCount > 0) {
      toast.warning(t('gallery.mobile.download.partialTitle'), {
        description: t('gallery.mobile.download.partialDescription', {
          failed: result.failedCount,
        }),
      })
    } else {
      toast.success(t('gallery.mobile.download.successTitle'))
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.mobile.download.failedTitle'), { description: message })
  } finally {
    isDownloading.value = false
  }
}

function handleDownloadConfirmationOpenChange(open: boolean) {
  downloadConfirmationOpen.value = open
  if (!open) {
    // 关闭确认框时丢弃尚未执行的下载快照。
    pendingDownloadIds.value = []
  }
}

// 使用确认框打开时保存的快照，不重新读取可能已经变化的当前选择。
async function confirmLargeDownload() {
  const assetIds = [...pendingDownloadIds.value]
  if (assetIds.length === 0) {
    downloadConfirmationOpen.value = false
    return
  }

  // 先关闭确认框并释放待执行状态，再开始准备归档。
  downloadConfirmationOpen.value = false
  pendingDownloadIds.value = []
  await startDownload(assetIds)
}

// 快照当前去重选择；超过阈值先确认，确认后仍使用这份快照。
async function handleDownload() {
  const assetIds = [...new Set(downloadAssetIds.value)].filter((assetId) => assetId > 0)
  if (isDownloading.value || assetIds.length === 0) {
    return
  }

  // 100 个只是防误操作阈值，后端仍允许无限量打包。
  if (assetIds.length > LARGE_DOWNLOAD_CONFIRMATION_THRESHOLD) {
    pendingDownloadIds.value = assetIds
    downloadConfirmationOpen.value = true
    return
  }

  await startDownload(assetIds)
}
</script>

<template>
  <div
    class="shrink-0 border-t border-border/70 bg-background/90 px-2 pt-2 pb-[calc(env(safe-area-inset-bottom)+0.5rem)] shadow-[0_-8px_24px_rgba(0,0,0,0.12)] backdrop-blur-md"
    @contextmenu.prevent.stop
  >
    <div class="mx-auto grid w-full max-w-xl grid-cols-5 gap-1">
      <Button
        variant="ghost"
        class="h-14 min-w-0 flex-col gap-1 rounded-lg px-1 text-xs"
        :disabled="downloadAssetIds.length === 0 || isDownloading || downloadConfirmationOpen"
        :aria-busy="isDownloading"
        @click="void handleDownload()"
      >
        <LoaderCircle v-if="isDownloading" class="size-5 animate-spin" />
        <Download v-else class="size-5" />
        <span>{{ t('gallery.mobile.actions.download') }}</span>
      </Button>

      <Button
        variant="ghost"
        class="h-14 min-w-0 flex-col gap-1 rounded-lg px-1 text-xs"
        :disabled="selectedCount === 0"
        @click="ratingSheetOpen = true"
      >
        <Star class="size-5" />
        <span>{{ t('gallery.mobile.actions.rating') }}</span>
      </Button>

      <Button
        variant="ghost"
        class="h-14 min-w-0 flex-col gap-1 rounded-lg px-1 text-xs"
        :class="allSelectedRejected ? 'text-rose-500' : ''"
        :disabled="selectedCount === 0"
        @click="handleRejected"
      >
        <X class="size-5" />
        <span>
          {{
            allSelectedRejected
              ? t('gallery.mobile.actions.cancelRejected')
              : t('gallery.mobile.actions.rejected')
          }}
        </span>
      </Button>

      <Button
        variant="ghost"
        class="h-14 min-w-0 flex-col gap-1 rounded-lg px-1 text-xs"
        :disabled="selectedCount === 0"
        @click="void openTagSheet()"
      >
        <Tag class="size-5" />
        <span>{{ t('gallery.mobile.actions.tags') }}</span>
      </Button>

      <Button
        variant="ghost"
        class="h-14 min-w-0 flex-col gap-1 rounded-lg px-1 text-xs"
        :disabled="selectedCount === 0"
        @click="moreSheetOpen = true"
      >
        <MoreHorizontal class="size-5" />
        <span>{{ t('gallery.mobile.actions.more') }}</span>
      </Button>
    </div>
  </div>

  <!-- 大批量下载只做防误操作确认，不限制后端实际可打包数量。 -->
  <AlertDialog :open="downloadConfirmationOpen" @update:open="handleDownloadConfirmationOpenChange">
    <AlertDialogContent>
      <AlertDialogHeader>
        <AlertDialogTitle>{{ t('gallery.mobile.download.confirmTitle') }}</AlertDialogTitle>
        <AlertDialogDescription>
          {{
            t('gallery.mobile.download.confirmDescription', {
              count: pendingDownloadIds.length,
            })
          }}
        </AlertDialogDescription>
      </AlertDialogHeader>
      <AlertDialogFooter>
        <AlertDialogCancel>{{ t('gallery.mobile.download.cancel') }}</AlertDialogCancel>
        <AlertDialogAction @click.prevent="void confirmLargeDownload()">
          {{ t('gallery.mobile.download.continue') }}
        </AlertDialogAction>
      </AlertDialogFooter>
    </AlertDialogContent>
  </AlertDialog>

  <Sheet v-model:open="ratingSheetOpen">
    <SheetContent
      side="bottom"
      class="rounded-t-2xl px-4 pb-[calc(env(safe-area-inset-bottom)+1rem)]"
      @contextmenu.prevent.stop
    >
      <SheetHeader class="px-0">
        <SheetTitle>{{ t('gallery.mobile.sheet.ratingTitle') }}</SheetTitle>
      </SheetHeader>
      <div class="grid grid-cols-6 gap-2">
        <Button
          v-for="rating in [0, 1, 2, 3, 4, 5]"
          :key="rating"
          variant="outline"
          class="h-12 flex-col gap-1 px-1"
          @click="handleRating(rating)"
        >
          <template v-if="rating === 0">
            <X class="size-4" />
            <span class="text-[11px]">{{ t('gallery.mobile.sheet.clearRating') }}</span>
          </template>
          <template v-else>
            <Star class="size-4 fill-amber-400 text-amber-400" />
            <span class="text-xs">{{ rating }}</span>
          </template>
        </Button>
      </div>
    </SheetContent>
  </Sheet>

  <Sheet v-model:open="tagSheetOpen">
    <SheetContent
      side="bottom"
      class="max-h-[82vh] overflow-y-auto rounded-t-2xl px-4 pb-[calc(env(safe-area-inset-bottom)+1rem)]"
      @contextmenu.prevent.stop
    >
      <SheetHeader class="px-0">
        <SheetTitle>{{ t('gallery.mobile.sheet.tagsTitle') }}</SheetTitle>
      </SheetHeader>
      <div
        v-if="tagLoading && store.tags.length === 0"
        class="py-8 text-center text-sm text-muted-foreground"
      >
        {{ t('gallery.sidebar.common.loading') }}
      </div>
      <TagSelectorPopover
        v-else
        :tags="store.tags"
        :selected-tag-ids="tagIds"
        @toggle="void handleTagToggle($event)"
      />
    </SheetContent>
  </Sheet>

  <Sheet v-model:open="moreSheetOpen">
    <SheetContent
      side="bottom"
      class="max-h-[86vh] overflow-y-auto rounded-t-2xl px-4 pb-[calc(env(safe-area-inset-bottom)+1rem)]"
      @contextmenu.prevent.stop
    >
      <SheetHeader class="px-0">
        <SheetTitle>{{ t('gallery.mobile.sheet.moreTitle') }}</SheetTitle>
      </SheetHeader>
      <div class="grid gap-1">
        <Button
          v-if="canUseLocalFileSystem"
          variant="ghost"
          class="h-12 justify-start gap-3 px-3"
          :disabled="!assetActions.isSingleSelection"
          @click="handleOpenAssetDefault"
        >
          <ExternalLink class="size-5" />
          {{ t('gallery.contextMenu.openDefaultApp.label') }}
        </Button>
        <Button
          v-if="canUseLocalFileSystem"
          variant="ghost"
          class="h-12 justify-start gap-3 px-3"
          :disabled="!assetActions.isSingleSelection"
          @click="handleRevealAssetInExplorer"
        >
          <FolderOpen class="size-5" />
          {{ t('gallery.contextMenu.revealInExplorer.label') }}
        </Button>
        <Button
          v-if="canUseLocalFileSystem"
          variant="ghost"
          class="h-12 justify-start gap-3 px-3"
          @click="handleCopyAssetsToClipboard"
        >
          <Copy class="size-5" />
          {{ t('gallery.contextMenu.copyFiles.label') }}
        </Button>
        <Button variant="ghost" class="h-12 justify-start gap-3 px-3" @click="handleMoveToFolder">
          <FolderOpen class="size-5" />
          {{ t('gallery.contextMenu.moveToFolder.label') }}
        </Button>
        <Button
          variant="ghost"
          class="h-12 justify-start gap-3 px-3"
          :disabled="!assetActions.canCopyTags"
          @click="handleCopyTags"
        >
          <Tag class="size-5" />
          {{ t('gallery.contextMenu.copyTags.label') }}
        </Button>
        <Button
          variant="ghost"
          class="h-12 justify-start gap-3 px-3"
          :disabled="!assetActions.canPasteTags"
          @click="handlePasteTags"
        >
          <Tag class="size-5" />
          {{ t('gallery.contextMenu.pasteTags.label') }}
        </Button>
        <Button
          variant="ghost"
          class="h-12 justify-start gap-3 px-3 text-destructive hover:text-destructive"
          @click="handleDelete"
        >
          <Trash2 class="size-5" />
          {{ assetActions.deleteMenuLabel }}
        </Button>
      </div>
    </SheetContent>
  </Sheet>
</template>
