<script setup lang="ts">
import { computed, ref, watch } from 'vue'
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
import { MobileDrawer } from '@/components/ui/mobile-drawer'
import { useGalleryAssetActions, useGalleryData } from '../../composables'
import { useGalleryStore } from '../../store'
import { galleryApi } from '../../api'
import TagSelectorPopover from '../tags/TagSelectorPopover.vue'
import { isWebView } from '@/core/env'

const props = withDefaults(
  defineProps<{
    variant?: 'solid' | 'immersive'
  }>(),
  {
    variant: 'solid',
  }
)

const containerClass = computed(() => {
  if (props.variant === 'immersive') {
    return 'shrink-0 bg-gradient-to-t from-background/60 via-background/40 to-transparent pt-6 pr-[max(0.5rem,var(--app-safe-right))] pb-[calc(var(--app-bottom-inset)+0.5rem)] pl-[max(0.5rem,var(--app-safe-left))] text-foreground'
  }
  return 'w-full bg-background/95 pt-2 pr-[max(0.5rem,var(--app-safe-right))] pb-[calc(var(--app-bottom-inset)+1rem)] pl-[max(0.5rem,var(--app-safe-left))] text-foreground dark:bg-popover/95'
})

const { t } = useI18n()
const { toast } = useToast()
const store = useGalleryStore()
const assetActions = useGalleryAssetActions()
const galleryData = useGalleryData()

const ratingSheetOpen = ref(false)
const tagSheetOpen = ref(false)
const moreSheetOpen = ref(false)
const tagIds = ref<number[]>([])
const isTogglingTag = ref(false)
const isDownloading = ref(false)
const downloadConfirmationOpen = ref(false)
const pendingDownloadIds = ref<number[]>([])

const LARGE_DOWNLOAD_CONFIRMATION_THRESHOLD = 100
const RATING_OPTIONS = [0, 1, 2, 3, 4, 5] as const

const selectedCount = computed(() => store.selectedCount)
const selectedAssets = computed(() => {
  const selectedIdSet = store.selection.selectedIds
  const assets = [] as {
    id: number
    reviewFlag?: 'none' | 'picked' | 'rejected'
    rating?: number
  }[]

  store.paginatedAssets.forEach((pageAssets) => {
    pageAssets.forEach((asset) => {
      if (selectedIdSet.has(asset.id)) {
        assets.push(asset)
      }
    })
  })

  return assets
})
const batchRatingSummary = ref<number | null | undefined>(undefined)
let batchRatingSummaryRequestVersion = 0

// 可见资产已经带有评分；选择集完整加载时直接计算，避免等待批量摘要请求造成文案闪烁。
const localBatchRatingSummary = computed<number | null | undefined>(() => {
  if (selectedCount.value <= 1 || selectedAssets.value.length !== selectedCount.value) {
    return undefined
  }

  const firstRating = selectedAssets.value[0]?.rating ?? 0
  return selectedAssets.value.every((asset) => (asset.rating ?? 0) === firstRating)
    ? firstRating
    : null
})

const singleSelectedAssetRating = computed<number | undefined>(() => {
  if (selectedCount.value !== 1) {
    return undefined
  }

  const selectedAsset = selectedAssets.value[0]
  if (selectedAsset) {
    return selectedAsset.rating ?? 0
  }

  const activeIndex = store.selection.activeIndex
  if (activeIndex !== undefined) {
    return store.getAssetAt(activeIndex)?.rating ?? 0
  }

  return undefined
})

const currentRating = computed<number | null | undefined>(() => {
  if (selectedCount.value === 1) {
    return singleSelectedAssetRating.value
  }
  if (selectedCount.value > 1) {
    const localRating = localBatchRatingSummary.value
    return localRating !== undefined ? localRating : batchRatingSummary.value
  }
  return undefined
})
const isRatingMixed = computed(() => selectedCount.value > 1 && currentRating.value === null)
const ratingDisplayLabel = computed(() => {
  if (isRatingMixed.value) {
    return t('gallery.mobile.actions.ratingMixed')
  }
  if (typeof currentRating.value === 'number') {
    return currentRating.value > 0
      ? t('gallery.mobile.actions.ratingValue', { rating: currentRating.value })
      : t('gallery.mobile.actions.ratingUnrated')
  }
  return t('gallery.mobile.actions.rating')
})

async function refreshBatchRatingSummary(assetIds: number[]) {
  const requestVersion = ++batchRatingSummaryRequestVersion

  if (assetIds.length <= 1) {
    batchRatingSummary.value = undefined
    return
  }

  try {
    const summary = await galleryApi.getBatchSelectionSummary(assetIds)
    if (requestVersion !== batchRatingSummaryRequestVersion) {
      return
    }
    batchRatingSummary.value = summary.rating
  } catch (error) {
    if (requestVersion === batchRatingSummaryRequestVersion) {
      batchRatingSummary.value = undefined
      console.warn('Failed to load mobile gallery rating summary:', error)
    }
  }
}

watch(
  () => [...assetActions.selectedAssetIds.value],
  (assetIds) => {
    void refreshBatchRatingSummary(assetIds)
  },
  { immediate: true }
)

watch(
  () => store.paginatedAssetsVersion,
  () => {
    if (assetActions.selectedAssetIds.value.length > 1) {
      void refreshBatchRatingSummary(assetActions.selectedAssetIds.value)
    }
  }
)

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

function openRatingSheet() {
  closeActionSheet()
  ratingSheetOpen.value = true
}

async function handleRating(rating: number) {
  const assetIds = [...assetActions.selectedAssetIds.value]
  closeActionSheet()

  // 使正在返回的旧批量摘要失效，避免用旧结果覆盖刚提交的评分。
  ++batchRatingSummaryRequestVersion

  try {
    if (rating === 0) {
      await assetActions.clearSelectedAssetsRating()
    } else {
      await assetActions.setSelectedAssetsRating(rating)
    }

    if (assetIds.length > 1 && isSameSelection(assetIds, assetActions.selectedAssetIds.value)) {
      batchRatingSummary.value = rating
    }
  } catch {
    if (assetActions.selectedAssetIds.value.length > 1) {
      void refreshBatchRatingSummary(assetActions.selectedAssetIds.value)
    }
  }
}

function isRatingSelected(rating: number): boolean {
  return typeof currentRating.value === 'number' && currentRating.value === rating
}

function isSameSelection(left: number[], right: number[]): boolean {
  if (left.length !== right.length) {
    return false
  }

  const rightIds = new Set(right)
  return left.every((id) => rightIds.has(id))
}

function handleRejected() {
  if (allSelectedRejected.value) {
    void assetActions.clearSelectedAssetsRejected()
    return
  }

  void assetActions.setSelectedAssetsRejected()
}

async function openTagSheet() {
  closeActionSheet()
  tagSheetOpen.value = true

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
  }
}

async function handleTagToggle(tagId: number) {
  if (isTogglingTag.value) {
    return
  }

  isTogglingTag.value = true
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
    isTogglingTag.value = false
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
  <div :class="containerClass" @contextmenu.prevent.stop>
    <div class="mx-auto grid w-full max-w-xl grid-cols-5 gap-1">
      <Button
        variant="ghost"
        class="h-13 min-w-0 flex-col gap-1 rounded-xl px-1 text-xs text-foreground transition-colors hover:bg-black/10 active:bg-black/15 disabled:opacity-40 dark:hover:bg-white/10 dark:active:bg-white/15"
        :disabled="downloadAssetIds.length === 0 || isDownloading || downloadConfirmationOpen"
        :aria-busy="isDownloading"
        @click="void handleDownload()"
      >
        <LoaderCircle v-if="isDownloading" class="size-5 animate-spin" :stroke-width="1.5" />
        <Download v-else class="size-5" :stroke-width="1.5" />
        <span>{{ t('gallery.mobile.actions.download') }}</span>
      </Button>

      <Button
        variant="ghost"
        class="h-13 min-w-0 flex-col gap-1 rounded-xl px-1 text-xs text-foreground transition-colors hover:bg-black/10 active:bg-black/15 disabled:opacity-40 dark:hover:bg-white/10 dark:active:bg-white/15"
        :disabled="selectedCount === 0"
        :aria-label="`${t('gallery.mobile.actions.rating')}: ${ratingDisplayLabel}`"
        @click="openRatingSheet"
      >
        <Star
          class="size-5 transition-colors"
          :stroke-width="1.5"
          :class="
            typeof currentRating === 'number' && currentRating > 0
              ? 'fill-amber-400 text-amber-400'
              : isRatingMixed
                ? 'text-amber-400'
                : ''
          "
        />
        <span>{{ ratingDisplayLabel }}</span>
      </Button>

      <Button
        variant="ghost"
        class="h-13 min-w-0 flex-col gap-1 rounded-xl px-1 text-xs text-foreground transition-colors hover:bg-black/10 active:bg-black/15 disabled:opacity-40 dark:hover:bg-white/10 dark:active:bg-white/15"
        :class="allSelectedRejected ? 'text-rose-500 hover:text-rose-600' : ''"
        :disabled="selectedCount === 0"
        @click="handleRejected"
      >
        <X class="size-5 scale-110" :stroke-width="1.5" />
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
        class="h-13 min-w-0 flex-col gap-1 rounded-xl px-1 text-xs text-foreground transition-colors hover:bg-black/10 active:bg-black/15 disabled:opacity-40 dark:hover:bg-white/10 dark:active:bg-white/15"
        :disabled="selectedCount === 0"
        @click="void openTagSheet()"
      >
        <Tag class="size-5" :stroke-width="1.5" />
        <span>{{ t('gallery.mobile.actions.tags') }}</span>
      </Button>

      <Button
        variant="ghost"
        class="h-13 min-w-0 flex-col gap-1 rounded-xl px-1 text-xs text-foreground transition-colors hover:bg-black/10 active:bg-black/15 disabled:opacity-40 dark:hover:bg-white/10 dark:active:bg-white/15"
        :disabled="selectedCount === 0"
        @click="moreSheetOpen = true"
      >
        <MoreHorizontal class="size-5" :stroke-width="1.5" />
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

  <MobileDrawer
    :open="ratingSheetOpen"
    side="bottom"
    :aria-label="t('gallery.mobile.sheet.ratingTitle')"
    class="rounded-t-2xl"
    @close="ratingSheetOpen = false"
  >
    <div class="px-4 pt-2 pb-4">
      <div class="mb-2 px-1 text-xs text-muted-foreground" aria-live="polite">
        {{ ratingDisplayLabel }}
      </div>
      <div class="grid grid-cols-6 gap-2">
        <Button
          v-for="rating in RATING_OPTIONS"
          :key="rating"
          variant="outline"
          class="h-12 flex-col gap-1 px-1"
          :class="
            isRatingSelected(rating)
              ? 'border-primary bg-sidebar-accent text-primary shadow-xs hover:bg-sidebar-accent'
              : ''
          "
          :aria-pressed="isRatingSelected(rating)"
          @click="handleRating(rating)"
        >
          <template v-if="rating === 0">
            <X class="size-4" />
            <span class="text-[11px]">{{ t('gallery.toolbar.filter.rating.unrated') }}</span>
          </template>
          <template v-else>
            <Star
              class="size-4"
              :class="
                isRatingSelected(rating)
                  ? 'fill-amber-400 text-amber-400'
                  : 'text-muted-foreground/50'
              "
            />
            <span class="text-xs">{{ rating }}</span>
          </template>
        </Button>
      </div>
    </div>
  </MobileDrawer>

  <MobileDrawer
    :open="tagSheetOpen"
    side="bottom"
    :aria-label="t('gallery.mobile.sheet.tagsTitle')"
    class="max-h-[82vh] overflow-y-auto rounded-t-2xl supports-[height:100dvh]:max-h-[82dvh]"
    @close="tagSheetOpen = false"
  >
    <div class="px-4 pb-4">
      <TagSelectorPopover
        :tags="store.tags"
        :selected-tag-ids="tagIds"
        @toggle="void handleTagToggle($event)"
      />
    </div>
  </MobileDrawer>

  <MobileDrawer
    :open="moreSheetOpen"
    side="bottom"
    class="max-h-[86vh] overflow-y-auto rounded-t-2xl pt-2 supports-[height:100dvh]:max-h-[86dvh]"
    @close="moreSheetOpen = false"
  >
    <div class="grid gap-1 px-4 pb-4">
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
  </MobileDrawer>
</template>
