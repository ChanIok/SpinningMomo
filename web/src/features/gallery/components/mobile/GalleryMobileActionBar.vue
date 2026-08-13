<script setup lang="ts">
import { computed, ref } from 'vue'
import { Copy, ExternalLink, FolderOpen, MoreHorizontal, Star, Tag, Trash2, X } from '@lucide/vue'
import { useI18n } from '@/composables/useI18n'
import { isLocalAccess } from '@/core/access'
import { Button } from '@/components/ui/button'
import { Sheet, SheetContent, SheetHeader, SheetTitle } from '@/components/ui/sheet'
import { useGalleryAssetActions, useGalleryData } from '../../composables'
import { useGalleryStore } from '../../store'
import { galleryApi } from '../../api'
import TagSelectorPopover from '../tags/TagSelectorPopover.vue'

const { t } = useI18n()
const store = useGalleryStore()
const assetActions = useGalleryAssetActions()
const galleryData = useGalleryData()

const ratingSheetOpen = ref(false)
const tagSheetOpen = ref(false)
const moreSheetOpen = ref(false)
const tagIds = ref<number[]>([])
const tagLoading = ref(false)

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
</script>

<template>
  <div
    class="shrink-0 border-t border-border/70 bg-background/90 px-2 pt-2 pb-[calc(env(safe-area-inset-bottom)+0.5rem)] shadow-[0_-8px_24px_rgba(0,0,0,0.12)] backdrop-blur-md"
    @contextmenu.prevent.stop
  >
    <div class="mx-auto grid w-full max-w-xl grid-cols-4 gap-1">
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
