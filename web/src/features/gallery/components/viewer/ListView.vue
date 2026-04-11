<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { ArrowDown, ArrowUp, ArrowUpDown } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import {
  useGallerySelection,
  useGalleryLightbox,
  useGalleryContextMenu,
  useGalleryView,
  useListVirtualizer,
} from '../../composables'
import type { Asset, SortBy, SortOrder } from '../../types'
import { prepareHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import AssetListRow from '../asset/AssetListRow.vue'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'
import ScrollBar from '@/components/ui/scroll-area/ScrollBar.vue'

// 行高 = viewSize * 0.4，使缩略图大小与网格视图的卡片尺寸保持视觉一致
const LIST_ROW_HEIGHT_FACTOR = 0.4
// 固定表头高度，需传给 virtualizer 作为 scrollPaddingStart，避免跳转时被表头遮挡
const LIST_HEADER_HEIGHT = 36

const { t } = useI18n()
const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()
const galleryContextMenu = useGalleryContextMenu()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const scrollContainerRef = ref<HTMLElement | null>(null)

const rowHeight = computed(() => Math.round(galleryView.viewSize.value * LIST_ROW_HEIGHT_FACTOR))

// headerHeight 作为 computed 传入 virtualizer，确保响应式
const headerHeight = computed(() => LIST_HEADER_HEIGHT)

// 缩略图尺寸略小于行高，留出上下内边距；最小 28px 保证可辨识
const thumbnailSize = computed(() => Math.max(28, rowHeight.value - 12))
// 缩略图列宽 = 缩略图尺寸 + 左右内边距
const thumbnailColumnWidth = computed(() => thumbnailSize.value + 20)
// 五列布局：缩略图 | 文件名（弹性） | 类型（固定） | 分辨率（固定） | 大小（固定）
const columnsTemplate = computed(
  () => `${thumbnailColumnWidth.value}px minmax(240px, 1fr) 88px 128px 108px`
)

const listVirtualizer = useListVirtualizer({
  containerRef: scrollContainerRef,
  rowHeight,
  scrollPaddingStart: headerHeight,
})

const sortBy = computed(() => galleryView.sortBy.value)
const sortOrder = computed(() => galleryView.sortOrder.value)

function getDefaultSortOrder(field: SortBy): SortOrder {
  // 名称默认升序，其余字段默认降序（大/新的排前面）
  switch (field) {
    case 'name':
      return 'asc'
    case 'resolution':
      return 'desc'
    case 'size':
      return 'desc'
    case 'createdAt':
    default:
      return 'desc'
  }
}

function handleSortHeaderClick(field: SortBy) {
  if (sortBy.value === field) {
    galleryView.toggleSortOrder()
    return
  }

  galleryView.setSorting(field, getDefaultSortOrder(field))
}

function getSortIcon(field: SortBy) {
  if (sortBy.value !== field) {
    return ArrowUpDown
  }

  return sortOrder.value === 'asc' ? ArrowUp : ArrowDown
}

function getSortButtonClass(field: SortBy): string {
  return sortBy.value === field ? 'text-foreground' : 'text-muted-foreground hover:text-foreground'
}

onMounted(async () => {
  // ScrollArea 的真实滚动容器在挂载后才可通过 viewportElement 获取
  if (scrollAreaRef.value) {
    scrollContainerRef.value = scrollAreaRef.value.viewportElement
  }

  await listVirtualizer.init()
})

function handleAssetClick(asset: Asset, event: MouseEvent, index: number) {
  void gallerySelection.handleAssetClick(asset, event, index)
}

function handleAssetDoubleClick(asset: Asset, event: MouseEvent, index: number) {
  const thumbnailEl = (event.target as HTMLElement).closest('[data-asset-thumbnail]')
  if (thumbnailEl) {
    const rect = thumbnailEl.getBoundingClientRect()
    const thumbnailUrl = galleryApi.getAssetThumbnailUrl(asset)
    prepareHero(rect, thumbnailUrl, asset.width ?? 1, asset.height ?? 1)
  }

  gallerySelection.handleAssetDoubleClick(asset, event)
  void galleryLightbox.openLightbox(index)
}

async function handleAssetContextMenu(asset: Asset, event: MouseEvent, index: number) {
  await gallerySelection.handleAssetContextMenu(asset, event, index)
  galleryContextMenu.openForAsset({ asset, event, index, sourceView: 'list' })
}

function scrollToIndex(index: number) {
  listVirtualizer.virtualizer.value.scrollToIndex(index, { align: 'auto' })
}

function getCardRect(index: number): DOMRect | null {
  const container = scrollContainerRef.value
  if (!container) {
    return null
  }

  // 通过 data-index 定位虚拟行，再取其内的缩略图元素位置，用于灯箱过渡动画
  const thumbnail = container.querySelector(
    `[data-index="${index}"] [data-asset-thumbnail]`
  ) as HTMLElement | null

  return thumbnail?.getBoundingClientRect() ?? null
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <ScrollArea ref="scrollAreaRef" type="always" class="mr-1 h-full">
    <template #scrollbar>
      <ScrollBar
        class="w-4 p-0.5"
        thumb-class="bg-muted-foreground/35 hover:bg-muted-foreground/50"
      />
    </template>
    <div class="px-4 pb-3">
      <div
        class="sticky top-0 z-10 grid items-center gap-3 border-b bg-background/95 px-3 backdrop-blur-sm"
        :style="{
          gridTemplateColumns: columnsTemplate,
          height: `${headerHeight}px`,
        }"
      >
        <div />
        <button
          type="button"
          class="flex items-center gap-1 text-left text-xs font-medium transition-colors"
          :class="getSortButtonClass('name')"
          @click="handleSortHeaderClick('name')"
        >
          <span>{{ t('gallery.details.asset.fileName') }}</span>
          <component :is="getSortIcon('name')" class="h-3.5 w-3.5" />
        </button>
        <div class="text-xs font-medium text-muted-foreground">
          {{ t('gallery.details.asset.type') }}
        </div>
        <button
          type="button"
          class="flex items-center gap-1 text-left text-xs font-medium transition-colors"
          :class="getSortButtonClass('resolution')"
          @click="handleSortHeaderClick('resolution')"
        >
          <span>{{ t('gallery.details.asset.resolution') }}</span>
          <component :is="getSortIcon('resolution')" class="h-3.5 w-3.5" />
        </button>
        <button
          type="button"
          class="ml-auto flex items-center justify-end gap-1 text-right text-xs font-medium transition-colors"
          :class="getSortButtonClass('size')"
          @click="handleSortHeaderClick('size')"
        >
          <span>{{ t('gallery.details.asset.fileSize') }}</span>
          <component :is="getSortIcon('size')" class="h-3.5 w-3.5" />
        </button>
      </div>

      <div
        class="relative"
        :style="{
          height: `${listVirtualizer.virtualizer.value.getTotalSize()}px`,
        }"
      >
        <div
          v-for="virtualItem in listVirtualizer.virtualItems.value"
          :key="virtualItem.index"
          :data-index="virtualItem.index"
          :style="{
            position: 'absolute',
            top: 0,
            left: 0,
            width: '100%',
            height: `${virtualItem.size}px`,
            transform: `translateY(${virtualItem.start}px)`,
          }"
        >
          <AssetListRow
            v-if="virtualItem.asset !== null"
            :asset="virtualItem.asset"
            :is-selected="gallerySelection.isAssetSelected(virtualItem.asset.id)"
            :row-height="rowHeight"
            :thumbnail-size="thumbnailSize"
            :columns-template="columnsTemplate"
            @click="(asset, event) => handleAssetClick(asset, event, virtualItem.index)"
            @double-click="
              (asset, event) => handleAssetDoubleClick(asset, event, virtualItem.index)
            "
            @context-menu="
              (asset, event) => void handleAssetContextMenu(asset, event, virtualItem.index)
            "
          />

          <div
            v-else
            class="grid animate-pulse items-center gap-3 rounded-sm px-3"
            :style="{
              gridTemplateColumns: columnsTemplate,
              height: `${rowHeight}px`,
            }"
          >
            <div
              class="rounded-sm bg-muted"
              :style="{ width: `${thumbnailSize}px`, height: `${thumbnailSize}px` }"
            />
            <div class="h-3 rounded bg-muted" />
            <div class="h-3 rounded bg-muted" />
            <div class="h-3 rounded bg-muted" />
            <div class="ml-auto h-3 w-20 rounded bg-muted" />
          </div>
        </div>
      </div>
    </div>
  </ScrollArea>
</template>
