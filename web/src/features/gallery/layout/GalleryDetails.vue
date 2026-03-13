<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Tooltip, TooltipContent, TooltipProvider, TooltipTrigger } from '@/components/ui/tooltip'
import { rgbToHex } from '@/components/ui/color-picker/colorUtils'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useGalleryStore } from '../store'
import { useGalleryData } from '../composables/useGalleryData'
import {
  getAssetMainColors,
  getAssetTags,
  removeTagsFromAsset,
  addTagsToAsset,
  getInfinityNikkiPhotoParams,
} from '../api'
import AssetDetailsContent from '../components/AssetDetailsContent.vue'
import TagSelectorPopover from '../components/TagSelectorPopover.vue'
import type { Asset, AssetMainColor, InfinityNikkiPhotoParams, Tag } from '../types'

const store = useGalleryStore()
const { t } = useI18n()
const { toast } = useToast()

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
const { getAssetThumbnailUrl } = useGalleryData()

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

const batchThumbnailUrl = computed(() => {
  if (!batchActiveAsset.value) return ''
  return getAssetThumbnailUrl(batchActiveAsset.value)
})

// 资产标签状态
const assetTags = ref<Tag[]>([])
const assetMainColors = ref<AssetMainColor[]>([])
const infinityNikkiPhotoParams = ref<InfinityNikkiPhotoParams | null>(null)
const hasMainColors = computed(() => assetMainColors.value.length > 0)
const hasInfinityNikkiDetails = computed(() => {
  const params = infinityNikkiPhotoParams.value
  if (!params) return false

  return (
    formatGameTime(params) !== null ||
    Boolean(params.cameraParams) ||
    formatNumber(params.cameraFocalLength) !== null ||
    params.apertureSection !== undefined ||
    formatMetadataText(params.filterId) !== null ||
    formatPercentage(params.filterStrength) !== null ||
    formatPercentage(params.vignetteIntensity) !== null ||
    formatMetadataText(params.lightId) !== null ||
    formatPercentage(params.lightStrength) !== null ||
    params.nikkiHidden !== undefined ||
    formatPoseId(params.poseId) !== null
  )
})

// 当前标签（详情面板焦点为 tag 时）
const currentTag = computed(() => {
  return detailsFocus.value.type === 'tag' ? detailsFocus.value.tag : null
})
const isRootTagSummary = computed(() => currentTag.value?.id === -1)
const rootTagCount = computed(() => store.tags.length)
const rootTagAssetTotalCount = computed(() => store.tagsAssetTotalCount)

// 监听 activeAsset 变化，加载详情数据
watch(
  activeAsset,
  async (asset) => {
    if (asset) {
      try {
        const [tags, mainColors, photoParams] = await Promise.all([
          getAssetTags(asset.id),
          getAssetMainColors(asset.id),
          asset.type === 'photo' || asset.type === 'live_photo'
            ? getInfinityNikkiPhotoParams(asset.id)
            : Promise.resolve(null),
        ])

        assetTags.value = tags
        assetMainColors.value = mainColors
        infinityNikkiPhotoParams.value = photoParams
      } catch (error) {
        console.error('Failed to load asset details:', error)
        assetTags.value = []
        assetMainColors.value = []
        infinityNikkiPhotoParams.value = null
      }
    } else {
      assetTags.value = []
      assetMainColors.value = []
      infinityNikkiPhotoParams.value = null
    }
  },
  { immediate: true }
)

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

    // 更新本地标签列表
    assetTags.value = assetTags.value.filter((tag) => tag.id !== tagId)
  } catch (error) {
    console.error('Failed to remove tag:', error)
  }
}

// 添加标签
async function handleAddTag(tagId: number) {
  if (!activeAsset.value) return

  // 检查是否已有此标签
  if (assetTags.value.some((tag) => tag.id === tagId)) {
    console.log('标签已存在')
    return
  }

  try {
    await addTagsToAsset({
      assetId: activeAsset.value.id,
      tagIds: [tagId],
    })

    // 重新加载标签列表
    assetTags.value = await getAssetTags(activeAsset.value.id)
  } catch (error) {
    console.error('Failed to add tag:', error)
  }
}

function padTwoDigits(value: number): string {
  return String(value).padStart(2, '0')
}

function formatGameTime(params: InfinityNikkiPhotoParams | null): string | null {
  if (!params) return null
  if (params.timeHour === undefined || params.timeMin === undefined) return null

  return `${padTwoDigits(params.timeHour)}:${padTwoDigits(params.timeMin)}`
}

function formatNumber(value: number | undefined, digits = 2): string | null {
  if (value === undefined) return null
  return Number(value)
    .toFixed(digits)
    .replace(/\.?0+$/, '')
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

async function handleCopyCameraParams(text: string) {
  let success = false

  if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(text)
      success = true
    } catch {
      success = false
    }
  }

  if (!success) {
    success = copyWithExecCommand(text)
  }

  if (success) {
    toast.success(t('gallery.details.infinityNikki.copyCameraParamsSuccess'))
    return
  }

  toast.error(t('gallery.details.infinityNikki.copyCameraParamsFailed'))
}

function formatPercentage(value: number | undefined): string | null {
  if (value === undefined) return null
  return `${formatNumber(value * 100, 1) ?? '0'}%`
}

function formatFocalLength(value: number | undefined): string | null {
  const formatted = formatNumber(value)
  return formatted ? `${formatted} mm` : null
}

function formatApertureSection(value: number | undefined): string | null {
  if (value === undefined) return null
  return `${value} ${t('gallery.details.infinityNikki.apertureSectionUnit')}`
}

function formatMetadataText(value: string | undefined): string | null {
  if (!value) return null
  const trimmed = value.trim()
  if (!trimmed || trimmed.toLowerCase() === 'none') return null
  return trimmed
}

function formatPoseId(value: number | undefined): string | null {
  if (value === undefined || value === 0) return null
  return String(value)
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
    <div class="p-4">
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
        <AssetDetailsContent :asset="activeAsset" :thumbnail-url="thumbnailUrl">
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

          <template #before-info>
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
                      @select="handleAddTag"
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
          </template>

          <template #after-info>
            <Separator />

            <template v-if="infinityNikkiPhotoParams && hasInfinityNikkiDetails">
              <div class="space-y-3">
                <h4 class="text-sm font-medium">{{ t('gallery.details.infinityNikki.title') }}</h4>
                <div class="space-y-2 text-xs">
                  <div
                    v-if="formatGameTime(infinityNikkiPhotoParams)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.gameTime')
                    }}</span>
                    <span>{{ formatGameTime(infinityNikkiPhotoParams) }}</span>
                  </div>

                  <div
                    v-if="infinityNikkiPhotoParams.cameraParams"
                    class="flex items-center justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.cameraParams')
                    }}</span>
                    <Button
                      variant="outline"
                      size="sm"
                      class="h-6 px-2 text-xs"
                      @click="handleCopyCameraParams(infinityNikkiPhotoParams.cameraParams)"
                    >
                      {{ t('gallery.details.infinityNikki.copyCameraParams') }}
                    </Button>
                  </div>

                  <div
                    v-if="formatFocalLength(infinityNikkiPhotoParams.cameraFocalLength)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.cameraFocalLength')
                    }}</span>
                    <span>{{ formatFocalLength(infinityNikkiPhotoParams.cameraFocalLength) }}</span>
                  </div>
                  <div
                    v-if="formatApertureSection(infinityNikkiPhotoParams.apertureSection)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.apertureSection')
                    }}</span>
                    <span>{{
                      formatApertureSection(infinityNikkiPhotoParams.apertureSection)
                    }}</span>
                  </div>
                  <div
                    v-if="formatMetadataText(infinityNikkiPhotoParams.filterId)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.filterId')
                    }}</span>
                    <span
                      class="max-w-32 truncate font-mono"
                      :title="formatMetadataText(infinityNikkiPhotoParams.filterId) ?? undefined"
                    >
                      {{ formatMetadataText(infinityNikkiPhotoParams.filterId) }}
                    </span>
                  </div>
                  <div
                    v-if="formatPercentage(infinityNikkiPhotoParams.filterStrength)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.filterStrength')
                    }}</span>
                    <span>{{ formatPercentage(infinityNikkiPhotoParams.filterStrength) }}</span>
                  </div>
                  <div
                    v-if="formatPercentage(infinityNikkiPhotoParams.vignetteIntensity)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.vignetteIntensity')
                    }}</span>
                    <span>{{ formatPercentage(infinityNikkiPhotoParams.vignetteIntensity) }}</span>
                  </div>
                  <div
                    v-if="formatMetadataText(infinityNikkiPhotoParams.lightId)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.lightId')
                    }}</span>
                    <span
                      class="max-w-32 truncate font-mono"
                      :title="formatMetadataText(infinityNikkiPhotoParams.lightId) ?? undefined"
                    >
                      {{ formatMetadataText(infinityNikkiPhotoParams.lightId) }}
                    </span>
                  </div>
                  <div
                    v-if="formatPercentage(infinityNikkiPhotoParams.lightStrength)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.lightStrength')
                    }}</span>
                    <span>{{ formatPercentage(infinityNikkiPhotoParams.lightStrength) }}</span>
                  </div>
                  <div
                    v-if="infinityNikkiPhotoParams.nikkiHidden !== undefined"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.nikkiHidden')
                    }}</span>
                    <span>{{
                      infinityNikkiPhotoParams.nikkiHidden ? t('common.yes') : t('common.no')
                    }}</span>
                  </div>
                  <div
                    v-if="formatPoseId(infinityNikkiPhotoParams.poseId)"
                    class="flex justify-between gap-2"
                  >
                    <span class="text-muted-foreground">{{
                      t('gallery.details.infinityNikki.poseId')
                    }}</span>
                    <span>{{ formatPoseId(infinityNikkiPhotoParams.poseId) }}</span>
                  </div>
                </div>
              </div>
            </template>

            <Separator v-if="hasInfinityNikkiDetails" />
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

        <template v-if="batchActiveAsset">
          <Separator />

          <h4 class="text-sm font-medium">{{ t('gallery.details.batch.currentFocus') }}</h4>
          <AssetDetailsContent :asset="batchActiveAsset" :thumbnail-url="batchThumbnailUrl" />
        </template>
        <div v-else class="text-xs text-muted-foreground">
          {{ t('gallery.details.batch.focusUnavailable') }}
        </div>

        <Separator />

        <div class="space-y-2">
          <p class="text-sm font-medium">{{ t('gallery.details.batch.title') }}</p>
          <div class="text-xs text-muted-foreground">
            {{ t('gallery.details.batch.placeholder') }}
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
