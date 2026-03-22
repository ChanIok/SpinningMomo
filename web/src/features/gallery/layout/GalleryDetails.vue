<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Separator } from '@/components/ui/separator'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { rgbToHex } from '@/components/ui/color-picker/colorUtils'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useSettingsStore } from '@/features/settings/store'
import { useGalleryStore } from '../store'
import { useGalleryData } from '../composables/useGalleryData'
import { useGalleryAssetActions } from '../composables'
import {
  getAssetMainColors,
  getAssetTags,
  removeTagsFromAsset,
  addTagsToAsset,
  getInfinityNikkiDetails,
  updateAssetDescription,
} from '../api'
import AssetDetailsContent from '../components/AssetDetailsContent.vue'
import AssetInfinityNikkiDetails from '../components/AssetInfinityNikkiDetails.vue'
import AssetHistogram from '../components/AssetHistogram.vue'
import AssetReviewControls from '../components/AssetReviewControls.vue'
import TagSelectorPopover from '../components/TagSelectorPopover.vue'
import type { Asset, AssetMainColor, InfinityNikkiDetails, Tag } from '../types'

const store = useGalleryStore()
const settingsStore = useSettingsStore()
const { t } = useI18n()
const { toast } = useToast()
const assetActions = useGalleryAssetActions()

// 获取详情面板焦点
const detailsFocus = computed(() => store.detailsPanel)

// 根据焦点直接获取对象引用
const currentFolder = computed(() => {
  return detailsFocus.value.type === 'folder' ? detailsFocus.value.folder : null
})
const isRootFolderSummary = computed(() => currentFolder.value?.id === -1)
const rootFolderCount = computed(() => store.folders.length)
const rootFolderAssetTotalCount = computed(() => store.foldersAssetTotalCount)

const activeAsset = computed(() => {
  return detailsFocus.value.type === 'asset' ? detailsFocus.value.asset : null
})

// 使用gallery数据composable
const { getAssetThumbnailUrl, getAssetUrl } = useGalleryData()

const selectedCount = computed(() => store.selectedCount)

function findLoadedAssetById(id: number): Asset | null {
  for (const pageAssets of store.paginatedAssets.values()) {
    const found = pageAssets.find((asset) => asset.id === id)
    if (found) {
      return found
    }
  }
  return null
}

const batchActiveAsset = computed(() => {
  if (detailsFocus.value.type !== 'batch') return null
  if (store.selection.selectedIds.size === 0) return null

  const activeIndex = store.selection.activeIndex
  if (activeIndex !== undefined) {
    const [currentAsset] = store.getAssetsInRange(activeIndex, activeIndex)
    if (currentAsset && store.selection.selectedIds.has(currentAsset.id)) {
      return currentAsset
    }
  }

  for (const id of store.selection.selectedIds) {
    const asset = findLoadedAssetById(id)
    if (asset) {
      return asset
    }
  }

  return null
})

// 计算缩略图URL
const thumbnailUrl = computed(() => {
  if (!activeAsset.value) return ''
  return getAssetThumbnailUrl(activeAsset.value)
})

const assetUrl = computed(() => {
  if (!activeAsset.value) return ''
  return getAssetUrl(activeAsset.value.id)
})

const batchThumbnailUrl = computed(() => {
  if (!batchActiveAsset.value) return ''
  return getAssetThumbnailUrl(batchActiveAsset.value)
})

const batchAssetUrl = computed(() => {
  if (!batchActiveAsset.value) return ''
  return getAssetUrl(batchActiveAsset.value.id)
})

const assetHistogramCacheKey = computed(() => {
  if (!activeAsset.value) return ''
  return activeAsset.value.hash ?? `${activeAsset.value.id}:${thumbnailUrl.value}`
})

const shouldShowAssetHistogram = computed(() => {
  if (!activeAsset.value) {
    return false
  }

  return (
    (activeAsset.value.type === 'photo' || activeAsset.value.type === 'live_photo') &&
    thumbnailUrl.value.length > 0
  )
})

const infinityNikkiEnabled = computed(
  () => settingsStore.appSettings.extensions.infinityNikki.enable
)

// 资产标签状态
const assetTags = ref<Tag[]>([])
const assetMainColors = ref<AssetMainColor[]>([])
const infinityNikkiDetails = ref<InfinityNikkiDetails | null>(null)
const hasMainColors = computed(() => assetMainColors.value.length > 0)

// 当前标签（详情面板焦点为 tag 时）
const currentTag = computed(() => {
  return detailsFocus.value.type === 'tag' ? detailsFocus.value.tag : null
})
const isRootTagSummary = computed(() => currentTag.value?.id === -1)
const rootTagCount = computed(() => store.tags.length)
const rootTagAssetTotalCount = computed(() => store.tagsAssetTotalCount)
const assetDescriptionDraft = ref('')
const isSavingAssetDescription = ref(false)

// 监听 activeAsset 变化，加载详情数据
watch(
  [activeAsset, infinityNikkiEnabled],
  async ([asset, nikkiEnabled]) => {
    if (asset) {
      try {
        const [tags, mainColors, details] = await Promise.all([
          getAssetTags(asset.id),
          getAssetMainColors(asset.id),
          nikkiEnabled ? getInfinityNikkiDetails(asset.id) : Promise.resolve(null),
        ])

        assetTags.value = tags
        assetMainColors.value = mainColors
        infinityNikkiDetails.value = details
      } catch (error) {
        console.error('Failed to load asset details:', error)
        assetTags.value = []
        assetMainColors.value = []
        infinityNikkiDetails.value = null
      }
    } else {
      assetTags.value = []
      assetMainColors.value = []
      infinityNikkiDetails.value = null
    }
  },
  { immediate: true }
)

watch(
  () => activeAsset.value?.id,
  () => {
    assetDescriptionDraft.value = activeAsset.value?.description ?? ''
  },
  { immediate: true }
)

async function reloadActiveAssetTags() {
  if (!activeAsset.value) {
    assetTags.value = []
    return
  }

  assetTags.value = await getAssetTags(activeAsset.value.id)
}

// Popover 状态
const showTagSelector = ref(false)

// 移除标签
async function handleRemoveTag(tagId: number) {
  if (!activeAsset.value) return

  try {
    await removeTagsFromAsset({
      assetId: activeAsset.value.id,
      tagIds: [tagId],
    })

    await reloadActiveAssetTags()
  } catch (error) {
    console.error('Failed to remove tag:', error)
  }
}

// 切换标签
async function handleToggleTag(tagId: number) {
  if (!activeAsset.value) return

  const hasTag = assetTags.value.some((tag) => tag.id === tagId)

  try {
    if (hasTag) {
      await removeTagsFromAsset({
        assetId: activeAsset.value.id,
        tagIds: [tagId],
      })
    } else {
      await addTagsToAsset({
        assetId: activeAsset.value.id,
        tagIds: [tagId],
      })
    }

    await reloadActiveAssetTags()
  } catch (error) {
    console.error('Failed to toggle tag:', error)
  }
}

async function handleSetRating(rating: number) {
  await assetActions.setSelectedAssetsRating(rating)
}

async function handleClearRating() {
  await assetActions.clearSelectedAssetsRating()
}

async function handleSetRejected() {
  await assetActions.setSelectedAssetsRejected()
}

async function handleClearRejected() {
  await assetActions.clearSelectedAssetsRejected()
}

function resetAssetDescriptionDraft() {
  assetDescriptionDraft.value = activeAsset.value?.description ?? ''
}

async function handleAssetDescriptionCommit() {
  if (!activeAsset.value || isSavingAssetDescription.value) {
    return
  }

  const normalizedDescription = assetDescriptionDraft.value.trim()
  const currentDescription = (activeAsset.value.description ?? '').trim()
  assetDescriptionDraft.value = normalizedDescription

  if (normalizedDescription === currentDescription) {
    return
  }

  isSavingAssetDescription.value = true

  try {
    await updateAssetDescription({
      assetId: activeAsset.value.id,
      description: normalizedDescription || undefined,
    })

    store.patchAssetDescription(activeAsset.value.id, normalizedDescription || undefined)
  } catch (error) {
    resetAssetDescriptionDraft()
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.details.asset.updateDescriptionFailed'), {
      description: message,
    })
  } finally {
    isSavingAssetDescription.value = false
  }
}

function handleInfinityNikkiDetailsUpdated(details: InfinityNikkiDetails) {
  infinityNikkiDetails.value = details
}

function copyWithExecCommand(text: string): boolean {
  if (typeof document === 'undefined') {
    return false
  }

  const textarea = document.createElement('textarea')
  textarea.value = text
  textarea.style.position = 'fixed'
  textarea.style.opacity = '0'
  textarea.style.pointerEvents = 'none'
  document.body.appendChild(textarea)
  textarea.focus()
  textarea.select()

  let success = false
  try {
    success = document.execCommand('copy')
  } catch {
    success = false
  }

  document.body.removeChild(textarea)
  return success
}

function formatNumber(value: number | undefined, digits = 2): string | null {
  if (value === undefined) return null
  return Number(value)
    .toFixed(digits)
    .replace(/\.?0+$/, '')
}

function formatPercentage(value: number | undefined): string | null {
  if (value === undefined) return null
  return `${formatNumber(value * 100, 1) ?? '0'}%`
}

function getColorHex(color: AssetMainColor): string {
  return rgbToHex(color.r, color.g, color.b)
}

async function handleCopyColorHex(color: AssetMainColor) {
  const hex = getColorHex(color)

  let success = false
  if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(hex)
      success = true
    } catch {
      success = false
    }
  }

  if (!success) {
    success = copyWithExecCommand(hex)
  }

  if (success) {
    toast.success(t('gallery.details.colors.copySuccess', { hex }))
    return
  }

  toast.error(t('gallery.details.colors.copyFailed'))
}
</script>

<template>
  <ScrollArea class="h-full">
    <div class="h-full p-4">
      <!-- 文件夹详情 -->
      <div v-if="detailsFocus.type === 'folder' && currentFolder" class="space-y-4">
        <div class="flex items-center justify-between">
          <h3 class="font-medium">{{ t('gallery.details.title') }}</h3>
        </div>

        <div v-if="isRootFolderSummary">
          <h4 class="mb-2 text-sm font-medium">
            {{ t('gallery.details.rootFolderSummary.title') }}
          </h4>
          <div class="space-y-2 text-xs">
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{
                t('gallery.details.rootFolderSummary.folderCount')
              }}</span>
              <span>{{ rootFolderCount }}</span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{
                t('gallery.details.rootFolderSummary.assetCount')
              }}</span>
              <span>{{ rootFolderAssetTotalCount }}</span>
            </div>
          </div>
        </div>

        <!-- 文件夹信息 -->
        <div v-else>
          <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.folderInfo') }}</h4>
          <div class="space-y-2 text-xs">
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{
                t('gallery.details.folderDisplayName')
              }}</span>
              <span
                class="truncate font-medium"
                :title="currentFolder.displayName || currentFolder.name"
              >
                {{ currentFolder.displayName || currentFolder.name }}
              </span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{ t('gallery.details.folderName') }}</span>
              <span class="truncate font-mono" :title="currentFolder.name">{{
                currentFolder.name
              }}</span>
            </div>
            <div class="flex flex-col gap-1">
              <span class="text-muted-foreground">{{ t('gallery.details.fullPath') }}</span>
              <p class="rounded bg-muted/50 p-2 font-mono text-xs break-all">
                {{ currentFolder.path }}
              </p>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{ t('gallery.details.assetCount') }}</span>
              <span>{{ t('gallery.details.itemCount', { count: currentFolder.assetCount }) }}</span>
            </div>
          </div>
        </div>
      </div>

      <!-- 资产详情 -->
      <div v-else-if="detailsFocus.type === 'asset' && activeAsset" class="space-y-4">
        <AssetDetailsContent
          :asset="activeAsset"
          :thumbnail-url="thumbnailUrl"
          :asset-url="assetUrl"
        >
          <template #after-preview>
            <div v-if="hasMainColors">
              <TooltipProvider>
                <div class="flex flex-wrap justify-center gap-2">
                  <Tooltip v-for="(color, index) in assetMainColors" :key="`${index}-${color.r}`">
                    <TooltipTrigger as-child>
                      <button
                        type="button"
                        class="h-5 w-5 shrink-0 rounded-sm border border-border/80 shadow-sm transition-transform hover:scale-[1.04]"
                        :style="{ backgroundColor: getColorHex(color) }"
                        @click="handleCopyColorHex(color)"
                      />
                    </TooltipTrigger>
                    <TooltipContent>
                      <p class="font-mono text-xs">{{ getColorHex(color) }}</p>
                      <p class="text-xs text-muted-foreground">
                        {{ formatPercentage(color.weight) }}
                      </p>
                    </TooltipContent>
                  </Tooltip>
                </div>
              </TooltipProvider>
            </div>
          </template>

          <template #description>
            <Input
              v-model="assetDescriptionDraft"
              :disabled="isSavingAssetDescription"
              :placeholder="t('gallery.details.asset.descriptionPlaceholder')"
              class="h-6 px-2 text-xs md:text-xs"
              @blur="handleAssetDescriptionCommit"
              @keydown.enter.prevent="handleAssetDescriptionCommit"
              @keydown.esc.prevent="resetAssetDescriptionDraft"
            />
          </template>

          <template #before-info>
            <div class="space-y-3">
              <div class="space-y-2">
                <h4 class="text-sm font-medium">{{ t('gallery.details.review.title') }}</h4>
                <AssetReviewControls
                  :rating="activeAsset.rating"
                  :review-flag="activeAsset.reviewFlag"
                  @set-rating="handleSetRating"
                  @clear-rating="handleClearRating"
                  @set-flag="handleSetRejected"
                  @clear-flag="handleClearRejected"
                />
              </div>

              <Separator />

              <div>
                <div class="mb-2 flex items-center justify-between">
                  <h4 class="text-sm font-medium">{{ t('gallery.details.tags.title') }}</h4>
                  <Popover v-model:open="showTagSelector">
                    <PopoverTrigger as-child>
                      <Button variant="ghost" size="sm" class="h-6 gap-1 px-2 text-xs">
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
                        >
                          <path d="M5 12h14" />
                          <path d="M12 5v14" />
                        </svg>
                        {{ t('gallery.details.tags.add') }}
                      </Button>
                    </PopoverTrigger>
                    <PopoverContent align="end" class="p-0">
                      <TagSelectorPopover
                        :tags="store.tags"
                        :selected-tag-ids="assetTags.map((t) => t.id)"
                        @toggle="handleToggleTag"
                      />
                    </PopoverContent>
                  </Popover>
                </div>

                <div v-if="assetTags.length > 0" class="flex flex-wrap gap-1.5">
                  <span
                    v-for="tag in assetTags"
                    :key="tag.id"
                    class="group inline-flex items-center gap-1 rounded bg-primary/10 px-2 py-1 text-xs text-primary transition-colors hover:bg-primary/20"
                  >
                    <span>{{ tag.name }}</span>
                    <button
                      class="flex h-3 w-3 items-center justify-center rounded-full opacity-60 transition-opacity hover:bg-primary/30 hover:opacity-100"
                      @click="handleRemoveTag(tag.id)"
                    >
                      <svg
                        xmlns="http://www.w3.org/2000/svg"
                        width="10"
                        height="10"
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="2"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      >
                        <path d="M18 6 6 18" />
                        <path d="m6 6 12 12" />
                      </svg>
                    </button>
                  </span>
                </div>
                <div v-else class="text-xs text-muted-foreground">
                  {{ t('gallery.details.tags.empty') }}
                </div>
              </div>
            </div>
          </template>

          <template #after-info>
            <AssetInfinityNikkiDetails
              v-if="infinityNikkiEnabled && infinityNikkiDetails && activeAsset"
              :asset-id="activeAsset.id"
              :details="infinityNikkiDetails"
              @updated="handleInfinityNikkiDetailsUpdated"
            />

            <template v-if="shouldShowAssetHistogram">
              <Separator />

              <AssetHistogram :cache-key="assetHistogramCacheKey" :image-url="thumbnailUrl" />
            </template>
          </template>
        </AssetDetailsContent>
      </div>

      <!-- 标签详情 -->
      <div v-else-if="detailsFocus.type === 'tag' && currentTag" class="space-y-4">
        <h3 class="font-medium">{{ t('gallery.details.title') }}</h3>

        <div v-if="isRootTagSummary">
          <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.rootTagSummary.title') }}</h4>
          <div class="space-y-2 text-xs">
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{
                t('gallery.details.rootTagSummary.tagCount')
              }}</span>
              <span>{{ rootTagCount }}</span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{
                t('gallery.details.rootTagSummary.assetCount')
              }}</span>
              <span>{{ rootTagAssetTotalCount }}</span>
            </div>
          </div>
        </div>

        <!-- 标签信息 -->
        <div v-else>
          <h4 class="mb-2 text-sm font-medium">{{ t('gallery.details.tagInfo') }}</h4>
          <div class="space-y-2 text-xs">
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{ t('gallery.details.tagName') }}</span>
              <span class="truncate font-medium" :title="currentTag.name">
                {{ currentTag.name }}
              </span>
            </div>
            <div v-if="currentTag.parentId" class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{ t('gallery.details.parentTagId') }}</span>
              <span>{{ currentTag.parentId }}</span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{ t('gallery.details.assetCount') }}</span>
              <span>{{ t('gallery.details.itemCount', { count: currentTag.assetCount }) }}</span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">{{ t('gallery.details.sortOrder') }}</span>
              <span>{{ currentTag.sortOrder }}</span>
            </div>
          </div>
        </div>
      </div>

      <!-- 批量操作 -->
      <div v-else-if="detailsFocus.type === 'batch'" class="space-y-4">
        <h3 class="font-medium">{{ t('gallery.details.batch.title') }}</h3>

        <div class="text-sm text-muted-foreground">
          {{ t('gallery.details.batch.selectedCount', { count: selectedCount }) }}
        </div>

        <div class="space-y-2">
          <h4 class="text-sm font-medium">{{ t('gallery.details.review.title') }}</h4>
          <AssetReviewControls
            :rating="batchActiveAsset?.rating ?? 0"
            :review-flag="batchActiveAsset?.reviewFlag ?? 'none'"
            :rating-indeterminate="true"
            @set-rating="handleSetRating"
            @clear-rating="handleClearRating"
            @set-flag="handleSetRejected"
            @clear-flag="handleClearRejected"
          />
        </div>

        <template v-if="batchActiveAsset">
          <Separator />

          <h4 class="text-sm font-medium">{{ t('gallery.details.batch.currentFocus') }}</h4>
          <AssetDetailsContent
            :asset="batchActiveAsset"
            :thumbnail-url="batchThumbnailUrl"
            :asset-url="batchAssetUrl"
          />
        </template>
        <div v-else class="text-xs text-muted-foreground">
          {{ t('gallery.details.batch.focusUnavailable') }}
        </div>

        <Separator />

        <div class="space-y-2">
          <p class="text-sm font-medium">{{ t('gallery.details.batch.helpTitle') }}</p>
          <div class="text-xs text-muted-foreground">
            {{ t('gallery.details.batch.reviewHint') }}
          </div>
        </div>
      </div>

      <!-- 空状态 -->
      <div v-else class="flex h-full items-center justify-center">
        <div class="text-center text-muted-foreground">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="48"
            height="48"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round"
            class="mx-auto mb-4 opacity-50"
          >
            <rect width="18" height="18" x="3" y="3" rx="2" ry="2" />
            <circle cx="9" cy="9" r="2" />
            <path d="m21 15-3.086-3.086a2 2 0 0 0-2.828 0L6 21" />
          </svg>
          <p class="text-sm">{{ t('gallery.details.empty') }}</p>
        </div>
      </div>
    </div>
  </ScrollArea>
</template>
