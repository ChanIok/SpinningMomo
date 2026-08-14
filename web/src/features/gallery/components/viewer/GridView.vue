<script setup lang="ts">
import { ref, computed, onMounted, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { useGalleryStore } from '../../store'
import type { Asset } from '../../types'
import {
  useGallerySelection,
  useGalleryLightbox,
  useGridVirtualizer,
  useCardImageScheduler,
  useTimelineRail,
  type CardImageScheduleItem,
} from '../../composables'
import { prepareHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import { useGalleryDragPayload } from '../../composables/useGalleryDragPayload'
import AssetCard from '../asset/AssetCard.vue'
import GalleryScrollbarRail from '../shell/GalleryScrollbarRail.vue'
import { useI18n } from '@/composables/useI18n'
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

const isTimelineMode = computed(() => store.isTimelineMode)
const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)
const columns = computed(() => {
  const itemSize = store.getEffectiveViewSize()
  return Math.max(1, Math.floor((containerWidth.value + gap) / (itemSize + gap)))
})
const gridCardSize = computed(() => {
  const totalGap = Math.max(0, columns.value - 1) * gap
  const availableWidth = containerWidth.value || scrollContainerRef.value?.clientWidth || 0

  return Math.max(1, Math.floor((availableWidth - totalGap) / Math.max(columns.value, 1)))
})

const gridVirtualizer = useGridVirtualizer({
  containerRef: scrollContainerRef,
  columns,
  containerWidth,
  gap,
})
const cardImageScheduler = useCardImageScheduler(
  scrollContainerRef,
  computed(() => store.view.useOriginalImagesForCards)
)

const { markers: railMarkers, labels: railLabels } = useTimelineRail({
  isTimelineMode,
  buckets: computed(() => store.timelineBuckets),
  locale,
  getOffsetByAssetIndex(assetIndex) {
    const rowIndex = Math.floor(assetIndex / Math.max(columns.value, 1))
    return rowIndex * gridVirtualizer.estimatedRowHeight.value
  },
})

onMounted(async () => {
  await gridVirtualizer.init()
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
  return gridVirtualizer.virtualRows.value.flatMap((row) =>
    row.assets.flatMap((asset) => {
      if (!asset) {
        return []
      }

      return [
        {
          assetId: asset.id,
          start: row.start,
          size: row.size,
          width: gridCardSize.value,
          height: gridCardSize.value,
        },
      ]
    })
  )
}

watch(
  () => gridVirtualizer.virtualRows.value,
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
    // 宽屏触摸保持桌面式选择语义；长按不额外切换到另一种模式。
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
  const row = Math.floor(index / columns.value)
  gridVirtualizer.virtualizer.value.scrollToIndex(row, { align: 'auto' })
}

function getCardRect(index: number): DOMRect | null {
  const container = scrollContainerRef.value
  if (!container) return null
  const cards = container.querySelectorAll('[data-asset-card]')
  const row = Math.floor(index / columns.value)
  const col = index % columns.value
  const virtualRows = gridVirtualizer.virtualRows.value
  const rowIdx = virtualRows.findIndex((r) => r.index === row)
  if (rowIdx === -1) return null
  const cardIndex = rowIdx * columns.value + col
  const card = cards[cardIndex]
  return card ? card.getBoundingClientRect() : null
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div class="relative flex h-full">
    <div
      ref="scrollContainerRef"
      :class="store.isCompactWindow ? 'px-0 py-0' : 'px-4 py-2 sm:pr-2'"
      class="hide-scrollbar flex-1 overflow-auto"
      @scroll="handleScroll"
    >
      <div
        :style="{
          height: `${gridVirtualizer.virtualizer.value.getTotalSize()}px`,
          position: 'relative',
        }"
      >
        <div
          v-for="virtualRow in gridVirtualizer.virtualRows.value"
          :key="virtualRow.index"
          :data-index="virtualRow.index"
          :style="{
            position: 'absolute',
            top: 0,
            left: 0,
            width: '100%',
            height: `${virtualRow.size}px`,
            transform: `translateY(${virtualRow.start}px)`,
          }"
        >
          <div
            class="grid"
            :style="{
              gridTemplateColumns: `repeat(${columns}, ${gridCardSize}px)`,
              gap: `${gap}px`,
              justifyContent: 'start',
            }"
          >
            <template
              v-for="(asset, idx) in virtualRow.assets"
              :key="asset?.id ?? `placeholder-${virtualRow.index}-${idx}`"
            >
              <AssetCard
                v-if="asset !== null"
                :asset="asset"
                :allow-thumbnail-load="cardImageScheduler.isThumbnailLoadAllowed(asset.id)"
                :allow-original-load="cardImageScheduler.isOriginalLoadAllowed(asset.id)"
                :original-preview-short-edge="gridCardSize"
                :is-selected="gallerySelection.isAssetSelected(asset.id)"
                @click="
                  (a, e, inputType) =>
                    handleAssetClick(a, e, virtualRow.index * columns + idx, inputType)
                "
                @long-press="(a, e) => handleAssetLongPress(a, e, virtualRow.index * columns + idx)"
                @double-click="
                  (a, e, inputType) =>
                    handleAssetDoubleClick(a, e, virtualRow.index * columns + idx, inputType)
                "
                @context-menu="
                  (a, e) => void handleAssetContextMenu(a, e, virtualRow.index * columns + idx)
                "
                @drag-start="(a, e) => handleAssetDragStart(a, e)"
              />

              <div
                v-else
                class="skeleton-card w-full"
                :class="!store.isCompactWindow && 'rounded'"
                :style="{ aspectRatio: '1 / 1' }"
              />
            </template>
          </div>
        </div>
      </div>
    </div>

    <GalleryScrollbarRail
      :container-height="containerHeight"
      :scroll-top="scrollTop"
      :viewport-height="containerHeight"
      :scroll-container="scrollContainerRef"
      :virtualizer="gridVirtualizer.virtualizer.value"
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

.skeleton-card {
  background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
  background-size: 200% 100%;
  animation: loading 1.5s ease-in-out infinite;
}

@keyframes loading {
  0% {
    background-position: 200% 0;
  }
  100% {
    background-position: -200% 0;
  }
}
</style>
