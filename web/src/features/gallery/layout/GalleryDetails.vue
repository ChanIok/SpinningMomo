<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { ScrollArea } from '@/components/ui/scroll-area'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../store'
import { useGalleryData } from '../composables/useGalleryData'
import { getAssetTags, removeTagsFromAsset, addTagsToAsset } from '../api'
import AssetDetailsContent from '../components/AssetDetailsContent.vue'
import TagSelectorPopover from '../components/TagSelectorPopover.vue'
import type { Asset, Tag } from '../types'

const store = useGalleryStore()
const { t } = useI18n()

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

function getLastSelectedId(): number | undefined {
  let lastId: number | undefined
  store.selection.selectedIds.forEach((id) => {
    lastId = id
  })
  return lastId
}

const batchActiveAsset = computed(() => {
  if (detailsFocus.value.type !== 'batch') return null
  if (store.selection.selectedIds.size === 0) return null

  const lastSelectedId = store.selection.lastSelectedId
  if (lastSelectedId !== undefined && store.selection.selectedIds.has(lastSelectedId)) {
    return findLoadedAssetById(lastSelectedId)
  }

  const focusIndex = store.selection.focusIndex
  if (focusIndex !== undefined) {
    const [focusAsset] = store.getAssetsInRange(focusIndex, focusIndex)
    if (focusAsset && store.selection.selectedIds.has(focusAsset.id)) {
      return focusAsset
    }
  }

  const fallbackSelectedId = getLastSelectedId()
  if (fallbackSelectedId === undefined) return null
  return findLoadedAssetById(fallbackSelectedId)
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

// 当前标签（详情面板焦点为 tag 时）
const currentTag = computed(() => {
  return detailsFocus.value.type === 'tag' ? detailsFocus.value.tag : null
})
const isRootTagSummary = computed(() => currentTag.value?.id === -1)
const rootTagCount = computed(() => store.tags.length)
const rootTagAssetTotalCount = computed(() => store.tagsAssetTotalCount)

// 监听 activeAsset 变化，加载标签
watch(
  activeAsset,
  async (asset) => {
    if (asset) {
      try {
        assetTags.value = await getAssetTags(asset.id)
      } catch (error) {
        console.error('Failed to load asset tags:', error)
        assetTags.value = []
      }
    } else {
      assetTags.value = []
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
</script>

<template>
  <ScrollArea class="h-full">
    <div class="p-4">
      <!-- 文件夹详情 -->
      <div v-if="detailsFocus.type === 'folder' && currentFolder" class="space-y-4">
        <div class="flex items-center justify-between">
          <h3 class="font-medium">详情</h3>
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
          <h4 class="mb-2 text-sm font-medium">文件夹信息</h4>
          <div class="space-y-2 text-xs">
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">显示名称</span>
              <span
                class="truncate font-medium"
                :title="currentFolder.displayName || currentFolder.name"
              >
                {{ currentFolder.displayName || currentFolder.name }}
              </span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">文件夹名</span>
              <span class="truncate font-mono" :title="currentFolder.name">{{
                currentFolder.name
              }}</span>
            </div>
            <div class="flex flex-col gap-1">
              <span class="text-muted-foreground">完整路径</span>
              <p class="rounded bg-muted/50 p-2 font-mono text-xs break-all">
                {{ currentFolder.path }}
              </p>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">资产数量</span>
              <span>{{ currentFolder.assetCount }} 项</span>
            </div>
          </div>
        </div>
      </div>

      <!-- 资产详情 -->
      <div v-else-if="detailsFocus.type === 'asset' && activeAsset" class="space-y-4">
        <h3 class="font-medium">详情</h3>
        <AssetDetailsContent :asset="activeAsset" :thumbnail-url="thumbnailUrl">
          <template #after-size>
            <Separator />

            <!-- 标签信息 -->
            <div>
              <div class="mb-2 flex items-center justify-between">
                <h4 class="text-sm font-medium">标签</h4>
                <!-- 添加标签按钮 -->
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
                      添加标签
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

              <!-- 标签列表 -->
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
              <div v-else class="text-xs text-muted-foreground">暂无标签</div>
            </div>
          </template>
        </AssetDetailsContent>
      </div>

      <!-- 标签详情 -->
      <div v-else-if="detailsFocus.type === 'tag' && currentTag" class="space-y-4">
        <h3 class="font-medium">详情</h3>

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
          <h4 class="mb-2 text-sm font-medium">标签信息</h4>
          <div class="space-y-2 text-xs">
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">标签名</span>
              <span class="truncate font-medium" :title="currentTag.name">
                {{ currentTag.name }}
              </span>
            </div>
            <div v-if="currentTag.parentId" class="flex justify-between gap-2">
              <span class="text-muted-foreground">父标签ID</span>
              <span>{{ currentTag.parentId }}</span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">资产数量</span>
              <span>{{ currentTag.assetCount }} 项</span>
            </div>
            <div class="flex justify-between gap-2">
              <span class="text-muted-foreground">排序顺序</span>
              <span>{{ currentTag.sortOrder }}</span>
            </div>
          </div>
        </div>
      </div>

      <!-- 批量操作 -->
      <div v-else-if="detailsFocus.type === 'batch'" class="space-y-4">
        <h3 class="font-medium">批量操作</h3>

        <div class="text-sm text-muted-foreground">已选中 {{ selectedCount }} 项</div>

        <template v-if="batchActiveAsset">
          <Separator />

          <h4 class="text-sm font-medium">当前焦点项</h4>
          <AssetDetailsContent :asset="batchActiveAsset" :thumbnail-url="batchThumbnailUrl" />
        </template>
        <div v-else class="text-xs text-muted-foreground">
          当前焦点项不可用（可能尚未加载或已被筛选条件过滤）
        </div>

        <Separator />

        <div class="space-y-2">
          <p class="text-sm font-medium">批量操作</p>
          <div class="text-xs text-muted-foreground">敬请期待...</div>
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
          <p class="text-sm">选择资产或文件夹查看详情</p>
        </div>
      </div>
    </div>
  </ScrollArea>
</template>
