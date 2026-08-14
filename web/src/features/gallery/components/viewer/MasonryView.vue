<script setup lang="ts">
import { computed, onMounted, ref, watch, type ComponentPublicInstance } from 'vue'
import { useElementSize } from '@vueuse/core'
import type { Asset } from '../../types'
import {
  useGallerySelection,
  useGalleryLightbox,
  useMasonryVirtualizer,
  useCardImageScheduler,
  useTimelineRail,
  type CardImageScheduleItem,
} from '../../composables'
import { prepareHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import { useGalleryDragPayload } from '../../composables/useGalleryDragPayload'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'
import AssetCard from '../asset/AssetCard.vue'
import GalleryScrollbarRail from '../shell/GalleryScrollbarRail.vue'
import { GALLERY_CARD_GAP, GALLERY_COMPACT_CARD_GAP } from '../../constants'
import { markGalleryScroll, shouldOpenAssetOnTap, type GalleryInputType } from '../../input'

const store = useGalleryStore()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()
const { prepareAssetDrag } = useGalleryDragPayload()
const { locale } = useI18n()

const scrollContainerRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)

const gap = store.isCompactWindow ? GALLERY_COMPACT_CARD_GAP : GALLERY_CARD_GAP

const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)
const targetColumnSize = computed(() => store.getEffectiveViewSize())
// 根据容器宽度和卡片目标尺寸计算列数，与 GridView 的算法保持一致
const columns = computed(() => {
  const itemSize = targetColumnSize.value
  return Math.max(1, Math.floor((containerWidth.value + gap) / (itemSize + gap)))
})

const masonryVirtualizer = useMasonryVirtualizer({
  containerRef: scrollContainerRef,
  columns,
  containerWidth,
  targetColumnSize,
  gap,
})
const cardImageScheduler = useCardImageScheduler(
  scrollContainerRef,
  computed(() => store.view.useOriginalImagesForCards)
)

const { markers: railMarkers, labels: railLabels } = useTimelineRail({
  isTimelineMode: computed(() => store.isTimelineMode),
  buckets: computed(() => store.timelineBuckets),
  locale,
  getOffsetByAssetIndex(assetIndex) {
    return masonryVirtualizer.itemStartByIndex.value.get(assetIndex)
  },
})

onMounted(async () => {
  await masonryVirtualizer.init()
})

// 同步滚动位置，并通知图片调度器进入滚动状态。
function handleScroll(event: Event) {
  const target = event.target as HTMLElement

  markGalleryScroll()
  // 滚动热路径优先小批量缩略图，原图增强等空闲后再升级。
  cardImageScheduler.markScrolling()
  scrollTop.value = target.scrollTop
}

// 收集当前虚拟窗口内的卡片，让调度器自己过滤真实可见区域。
function getCardImageScheduleItems(): CardImageScheduleItem[] {
  return masonryVirtualizer.virtualItems.value.flatMap((item) => {
    if (!item.asset) {
      return []
    }

    return [
      {
        assetId: item.asset.id,
        start: item.start,
        size: item.size,
        width: masonryVirtualizer.columnWidth.value,
        height: item.size,
      },
    ]
  })
}

watch(
  () => masonryVirtualizer.virtualItems.value,
  () => {
    // 虚拟项变化时只提交候选列表；缩略图和原图分别按优先级派发。
    cardImageScheduler.scheduleVisibleItems(getCardImageScheduleItems())
  },
  { immediate: true }
)

function getAssetAspectRatio(asset: Asset | null): string {
  if (!asset || !asset.width || !asset.height || asset.width <= 0 || asset.height <= 0) {
    return '1 / 1'
  }

  return `${asset.width} / ${asset.height}`
}

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

function scrollToIndex(index: number) {
  masonryVirtualizer.virtualizer.value.scrollToIndex(index, { align: 'auto' })
}

function getCardRect(index: number): DOMRect | null {
  const container = scrollContainerRef.value
  if (!container) {
    return null
  }

  // 通过 data-index 定位虚拟项，再取其内的卡片元素位置，用于灯箱过渡动画
  const card = container.querySelector(
    `[data-index="${index}"] [data-asset-card]`
  ) as HTMLElement | null

  return card?.getBoundingClientRect() ?? null
}

function measureItemElement(element: Element | ComponentPublicInstance | null) {
  // Vue 的 :ref 回调可能传入组件实例，过滤后只将原生 HTMLElement 交给 virtualizer 实测
  if (element instanceof HTMLElement || element === null) {
    masonryVirtualizer.measureElement(element)
  }
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div class="relative flex h-full">
    <div
      ref="scrollContainerRef"
      :class="store.isCompactWindow ? 'px-0 py-0' : 'px-4 py-2 sm:pr-2'"
      class="hide-scrollbar h-full flex-1 overflow-auto"
      @scroll="handleScroll"
    >
      <div>
        <div
          :style="{
            height: `${masonryVirtualizer.virtualizer.value.getTotalSize()}px`,
            position: 'relative',
          }"
        >
          <div
            v-for="virtualItem in masonryVirtualizer.virtualItems.value"
            :key="virtualItem.asset?.id ?? `placeholder-${virtualItem.index}`"
            :ref="measureItemElement"
            :data-index="virtualItem.index"
            :style="{
              position: 'absolute',
              top: 0,
              left: 0,
              width: `${masonryVirtualizer.columnWidth.value}px`,
              height: `${virtualItem.size}px`,
              transform: `translateX(${masonryVirtualizer.getLaneOffset(virtualItem.lane)}px) translateY(${virtualItem.start}px)`,
            }"
          >
            <AssetCard
              v-if="virtualItem.asset !== null"
              :asset="virtualItem.asset"
              :aspect-ratio="getAssetAspectRatio(virtualItem.asset)"
              :allow-thumbnail-load="
                cardImageScheduler.isThumbnailLoadAllowed(virtualItem.asset.id)
              "
              :allow-original-load="cardImageScheduler.isOriginalLoadAllowed(virtualItem.asset.id)"
              :original-preview-short-edge="
                Math.min(masonryVirtualizer.columnWidth.value, virtualItem.size)
              "
              :is-selected="gallerySelection.isAssetSelected(virtualItem.asset.id)"
              :style="{
                height: `${virtualItem.size}px`,
              }"
              @click="
                (asset, event, inputType) =>
                  handleAssetClick(asset, event, virtualItem.index, inputType)
              "
              @long-press="(asset, event) => handleAssetLongPress(asset, event, virtualItem.index)"
              @double-click="
                (asset, event, inputType) =>
                  handleAssetDoubleClick(asset, event, virtualItem.index, inputType)
              "
              @context-menu="
                (asset, event) => void handleAssetContextMenu(asset, event, virtualItem.index)
              "
              @drag-start="(asset, event) => handleAssetDragStart(asset, event)"
            />

            <div
              v-else
              class="animate-pulse bg-muted"
              :class="!store.isCompactWindow && 'rounded'"
              :style="{
                width: '100%',
                height: `${masonryVirtualizer.getAssetHeight(null, virtualItem.index)}px`,
              }"
            />
          </div>
        </div>
      </div>
    </div>

    <GalleryScrollbarRail
      :container-height="containerHeight"
      :scroll-top="scrollTop"
      :viewport-height="containerHeight"
      :scroll-container="scrollContainerRef"
      :virtualizer="masonryVirtualizer.virtualizer.value"
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
