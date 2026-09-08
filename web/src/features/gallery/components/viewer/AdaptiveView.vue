<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from 'vue'
import type { Asset } from '../../types'
import {
  useAdaptiveVirtualizer,
  useGallerySelection,
  useGalleryLightbox,
  useCardImageScheduler,
  useTimelineRail,
  useGalleryVirtualScrollMargin,
  useGalleryViewerSize,
  type CardImageScheduleItem,
} from '../../composables'
import { prepareHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import { useGalleryDragPayload } from '../../composables/useGalleryDragPayload'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'
import AssetCard from '../asset/AssetCard.vue'
import GalleryScrollbarRail from '../shell/GalleryScrollbarRail.vue'
import GalleryHeroHeader from '../shell/GalleryHeroHeader.vue'
import GalleryTimelineHeader from '../shell/GalleryTimelineHeader.vue'
import { GALLERY_CARD_GAP, GALLERY_COMPACT_CARD_GAP } from '../../constants'
import { markGalleryScroll, shouldOpenAssetOnTap, type GalleryInputType } from '../../input'

const store = useGalleryStore()
const props = withDefaults(
  defineProps<{
    toolbarHeight?: number
    initialAnchorIndex?: number
  }>(),
  {
    toolbarHeight: 0,
  }
)
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()
const { prepareAssetDrag } = useGalleryDragPayload()
const { locale } = useI18n()

const scrollContainerRef = ref<HTMLElement | null>(null)
const heroHeaderRef = ref<HTMLElement | null>(null)
const scrollContentRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)
const gap = store.isCompactWindow ? GALLERY_COMPACT_CARD_GAP : GALLERY_CARD_GAP
const targetRowHeight = computed(() => store.getEffectiveViewSize())
const toolbarHeight = computed(() => props.toolbarHeight)
const { scrollMargin } = useGalleryVirtualScrollMargin(
  scrollContainerRef,
  heroHeaderRef,
  toolbarHeight
)
const isMultiSelectMode = computed(() => store.selection.mode === 'multi-select')

// AdaptiveView 不再依赖 ScrollArea，避免第三方滚动容器内部测量语义干扰 thumb 尺寸。
const { width: containerWidth, height: containerHeight } = useGalleryViewerSize(scrollContainerRef)

const adaptiveVirtualizer = useAdaptiveVirtualizer({
  containerRef: scrollContainerRef,
  containerWidth,
  targetRowHeight,
  scrollMargin,
  gap,
})
const cardImageScheduler = useCardImageScheduler(
  scrollContainerRef,
  computed(() => store.view.useOriginalImagesForCards),
  computed(() => store.isCompactWindow)
)

const { markers: railMarkers, labels: railLabels } = useTimelineRail({
  isTimelineMode: computed(() => store.isTimelineMode),
  buckets: computed(() => store.timelineBuckets),
  locale,
  getOffsetByAssetIndex(assetIndex) {
    const rowIndex = adaptiveVirtualizer.rowIndexByAssetIndex.value.get(assetIndex)
    if (rowIndex === undefined) {
      return undefined
    }

    const rowStart = adaptiveVirtualizer.rows.value[rowIndex]?.start
    return rowStart === undefined ? undefined : scrollMargin.value + rowStart
  },
})

onMounted(async () => {
  // 等滚动容器与虚拟布局完成挂载更新后，再恢复切换锚点。
  await nextTick()
  if (props.initialAnchorIndex !== undefined && props.initialAnchorIndex > 0) {
    scrollToIndex(props.initialAnchorIndex, 'start')
  }
  await adaptiveVirtualizer.init()
})

// 同步滚动位置，并通知图片调度器进入滚动状态。
function handleScroll(event: Event) {
  // 轨道指示器与 hover 映射都依赖真实 scrollTop，因此这里直接从原生容器同步。
  const target = event.target as HTMLElement

  markGalleryScroll()
  // 滚动热路径优先小批量缩略图，原图增强等空闲后再升级。
  cardImageScheduler.markScrolling()
  scrollTop.value = target.scrollTop
  if (store.isCompactWindow) {
    store.handleCompactScroll(target.scrollTop)
  }
}

// 收集当前虚拟窗口内的卡片，让调度器自己过滤真实可见区域。
function getCardImageScheduleItems(): CardImageScheduleItem[] {
  return adaptiveVirtualizer.virtualRows.value.flatMap((row) =>
    row.items.flatMap((item) => {
      if (!item.asset) {
        return []
      }

      return [
        {
          assetId: item.asset.id,
          start: row.start,
          size: row.size,
          width: item.width,
          height: item.height,
        },
      ]
    })
  )
}

watch(
  () => adaptiveVirtualizer.virtualRows.value,
  () => {
    // 虚拟项变化时只提交候选列表；缩略图和原图分别按优先级派发。
    cardImageScheduler.scheduleVisibleItems(getCardImageScheduleItems())
  },
  { immediate: true }
)

function handleAssetClick(
  asset: Asset,
  event: MouseEvent,
  index: number,
  inputType: GalleryInputType
) {
  if (store.selection.mode === 'multi-select') {
    void gallerySelection.toggleIndex(index, asset)
    return
  }

  if (shouldOpenAssetOnTap(store.isCompactWindow, inputType)) {
    openAssetLightbox(asset, event, index, inputType)
    return
  }

  void gallerySelection.handleAssetClick(asset, event, index)
}

function handleAssetLongPress(asset: Asset, _event: PointerEvent, index: number) {
  if (store.selection.mode === 'multi-select') {
    return
  }

  if (!store.isCompactWindow) {
    void gallerySelection.selectOnlyIndex(index)
    return
  }

  gallerySelection.enterMultiSelectMode(asset, index)
}

function handleAssetDoubleClick(
  asset: Asset,
  event: MouseEvent,
  index: number,
  inputType: GalleryInputType
) {
  openAssetLightbox(asset, event, index, inputType)
}

function openAssetLightbox(
  asset: Asset,
  event: MouseEvent,
  index: number,
  inputType: GalleryInputType
) {
  const cardEl = (event.target as HTMLElement).closest('[data-asset-card]')
  if (cardEl) {
    const rect = cardEl.getBoundingClientRect()
    const thumbnailUrl = galleryApi.getAssetThumbnailUrl(asset)
    prepareHero(rect, thumbnailUrl, asset.width ?? 1, asset.height ?? 1)
  }

  void galleryLightbox.openLightbox(index, inputType)
}

async function handleAssetContextMenu(asset: Asset, event: MouseEvent, index: number) {
  await gallerySelection.handleAssetContextMenu(asset, event, index)
  store.openContextMenuForAsset(event)
}

function handleAssetDragStart(asset: Asset, event: DragEvent) {
  prepareAssetDrag(event, asset.id)
}

function scrollToIndex(index: number, align: 'auto' | 'start' = 'auto') {
  adaptiveVirtualizer.scrollToIndex(index, align)
}

function getCardRect(index: number): DOMRect | null {
  const container = scrollContainerRef.value
  if (!container) {
    return null
  }

  // 统一通过 data-index 找到当前已渲染卡片，供灯箱 hero / reverse-hero 动画复用。
  const card = container.querySelector(
    `[data-index="${index}"] [data-asset-card]`
  ) as HTMLElement | null

  return card?.getBoundingClientRect() ?? null
}

function getTopVisibleAssetIndex(): number {
  const container = scrollContainerRef.value
  if (!container) return 0

  const viewportStart = container.scrollTop
  const viewportEnd = viewportStart + container.clientHeight
  const rows = adaptiveVirtualizer.virtualRows.value
  for (const row of rows) {
    // 跳过分组标题和视口外的预渲染行。
    if (
      row.kind === 'assets' &&
      row.items[0] !== undefined &&
      row.start + row.size > viewportStart &&
      row.start < viewportEnd
    ) {
      return row.items[0].index
    }
  }
  return 0
}

defineExpose({ scrollToIndex, getCardRect, getTopVisibleAssetIndex })
</script>

<template>
  <div class="relative flex h-full">
    <div
      ref="scrollContainerRef"
      :class="[
        store.isCompactWindow ? 'px-0 pt-[var(--gallery-toolbar-height)]' : 'px-4 py-2 sm:pr-2',
        isMultiSelectMode || store.isCompactWindow
          ? 'pb-[calc(max(var(--app-bottom-inset),var(--gallery-action-bar-height))+0.5rem)]'
          : '',
      ]"
      class="hide-scrollbar flex-1 overflow-auto"
      @scroll="handleScroll"
    >
      <div ref="scrollContentRef">
        <div ref="heroHeaderRef">
          <GalleryHeroHeader />
        </div>
        <div class="pb-3">
          <div
            :style="{
              height: `${adaptiveVirtualizer.virtualizer.value.getTotalSize()}px`,
              position: 'relative',
            }"
          >
            <div
              v-for="virtualRow in adaptiveVirtualizer.virtualRows.value"
              :key="virtualRow.index"
              :style="{
                position: 'absolute',
                top: 0,
                left: 0,
                width: '100%',
                height: `${virtualRow.size}px`,
                transform: `translateY(${virtualRow.start - scrollMargin}px)`,
                display: virtualRow.kind === 'assets' ? 'flex' : 'block',
                gap: virtualRow.kind === 'assets' ? `${adaptiveVirtualizer.gap}px` : undefined,
              }"
            >
              <GalleryTimelineHeader
                v-if="virtualRow.kind === 'month' || virtualRow.kind === 'day'"
                :kind="virtualRow.kind"
                :date="virtualRow.date"
                :month="virtualRow.month ?? ''"
                :count="virtualRow.count ?? 0"
                :compact="store.isCompactWindow"
                :is-multi-select="isMultiSelectMode"
                :start-index="virtualRow.startIndex"
                :end-index="virtualRow.endIndex"
                :is-all-selected="
                  gallerySelection.isRangeAllSelected(virtualRow.startIndex, virtualRow.endIndex)
                "
                @select-group="
                  gallerySelection.toggleRangeSelection($event.startIndex, $event.endIndex)
                "
              />

              <template v-for="item in virtualRow.items" :key="item.id">
                <div
                  :data-index="item.index"
                  class="shrink-0"
                  :style="{ width: `${item.width}px`, height: `${item.height}px` }"
                >
                  <AssetCard
                    v-if="item.asset !== null"
                    :asset="item.asset"
                    :aspect-ratio="`${item.width} / ${item.height}`"
                    :allow-thumbnail-load="cardImageScheduler.isThumbnailLoadAllowed(item.asset.id)"
                    :allow-original-load="cardImageScheduler.isOriginalLoadAllowed(item.asset.id)"
                    :original-preview-short-edge="Math.min(item.width, item.height)"
                    :is-selected="gallerySelection.isAssetSelected(item.asset.id)"
                    @click="
                      (asset, event, inputType) =>
                        handleAssetClick(asset, event, item.index, inputType)
                    "
                    @long-press="(asset, event) => handleAssetLongPress(asset, event, item.index)"
                    @double-click="
                      (asset, event, inputType) =>
                        handleAssetDoubleClick(asset, event, item.index, inputType)
                    "
                    @context-menu="
                      (asset, event) => void handleAssetContextMenu(asset, event, item.index)
                    "
                    @drag-start="(asset, event) => handleAssetDragStart(asset, event)"
                  />

                  <div
                    v-else
                    class="h-full w-full animate-pulse bg-muted"
                    :class="!store.isCompactWindow && 'rounded-sm'"
                  />
                </div>
              </template>
            </div>
          </div>
        </div>
      </div>
    </div>

    <GalleryScrollbarRail
      :container-height="containerHeight"
      :scroll-top="scrollTop"
      :viewport-height="containerHeight"
      :scroll-container="scrollContainerRef"
      :content-element="scrollContentRef"
      :virtualizer="adaptiveVirtualizer.virtualizer.value"
      :markers="railMarkers"
      :labels="railLabels"
    />
  </div>
</template>

<style scoped>
.hide-scrollbar::-webkit-scrollbar {
  display: none;
}

.hide-scrollbar {
  scrollbar-width: none;
}
</style>
