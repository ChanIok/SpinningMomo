<script setup lang="ts">
import { ref, computed, onMounted, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { useGalleryStore } from '../../store'
import type { Asset } from '../../types'
import {
  useGalleryView,
  useGallerySelection,
  useGalleryLightbox,
  useGalleryContextMenu,
  useGridVirtualizer,
  useTimelineRail,
} from '../../composables'
import { prepareHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import { useGalleryDragPayload } from '../../composables/useGalleryDragPayload'
import AssetCard from '../asset/AssetCard.vue'
import GridTimelineRailBridge from './GridTimelineRailBridge.vue'
import { useI18n } from '@/composables/useI18n'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'

const store = useGalleryStore()
const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()
const galleryContextMenu = useGalleryContextMenu()
const { prepareAssetDrag } = useGalleryDragPayload()
const { locale } = useI18n()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const scrollContainerRef = ref<HTMLElement | null>(null)

const isTimelineMode = computed(() => store.isTimelineMode)
const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)
const columns = computed(() => {
  const itemSize = galleryView.viewSize.value
  const gap = 16
  return Math.max(1, Math.floor((containerWidth.value + gap) / (itemSize + gap)))
})

const gridVirtualizer = useGridVirtualizer({
  containerRef: scrollContainerRef,
  columns,
  containerWidth,
})

const { markers: railMarkers, labels: railLabels } = useTimelineRail({
  isTimelineMode,
  buckets: computed(() => store.timelineBuckets),
  locale,
  getOffsetByAssetIndex(assetIndex) {
    const rowIndex = Math.floor(assetIndex / Math.max(columns.value, 1))
    return rowIndex * gridVirtualizer.estimatedRowHeight.value
  },
})

watch(isTimelineMode, async (newValue) => {
  setTimeout(async () => {
    if (!newValue && scrollAreaRef.value) {
      scrollContainerRef.value = scrollAreaRef.value.viewportElement
    }
    await gridVirtualizer.init()
  }, 1000)
})

onMounted(async () => {
  if (!isTimelineMode.value && scrollAreaRef.value) {
    scrollContainerRef.value = scrollAreaRef.value.viewportElement
  }

  await gridVirtualizer.init()
})

function handleAssetClick(asset: Asset, event: MouseEvent, index: number) {
  void gallerySelection.handleAssetClick(asset, event, index)
}

function handleAssetDoubleClick(asset: Asset, event: MouseEvent, index: number) {
  const cardEl = (event.target as HTMLElement).closest('[data-asset-card]')
  if (cardEl) {
    const rect = cardEl.getBoundingClientRect()
    const thumbnailUrl = galleryApi.getAssetThumbnailUrl(asset)
    prepareHero(rect, thumbnailUrl, asset.width ?? 1, asset.height ?? 1)
  }
  gallerySelection.handleAssetDoubleClick(asset, event)
  void galleryLightbox.openLightbox(index)
}

async function handleAssetContextMenu(asset: Asset, event: MouseEvent, index: number) {
  await gallerySelection.handleAssetContextMenu(asset, event, index)
  galleryContextMenu.openForAsset({ asset, event, index, sourceView: 'grid' })
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
  // 虚拟列表只渲染可见行，找到与 index 对应的卡片
  const row = Math.floor(index / columns.value)
  const col = index % columns.value
  const virtualRows = gridVirtualizer.virtualRows.value
  const rowIdx = virtualRows.findIndex((r) => r.index === row)
  if (rowIdx === -1) return null
  // 每行有 columns 个卡片，从已渲染的 cards 中定位
  const cardIndex = rowIdx * columns.value + col
  const card = cards[cardIndex]
  return card ? card.getBoundingClientRect() : null
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div v-if="isTimelineMode" class="flex h-full">
    <div ref="scrollContainerRef" class="hide-scrollbar flex-1 overflow-auto py-2 pr-2 pl-4">
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
            class="grid justify-items-center gap-3"
            :style="{
              gridTemplateColumns: `repeat(${columns}, 1fr)`,
            }"
          >
            <template
              v-for="(asset, idx) in virtualRow.assets"
              :key="asset?.id ?? `placeholder-${virtualRow.index}-${idx}`"
            >
              <AssetCard
                v-if="asset !== null"
                :asset="asset"
                :is-selected="gallerySelection.isAssetSelected(asset.id)"
                @click="(a, e) => handleAssetClick(a, e, virtualRow.index * columns + idx)"
                @double-click="
                  (a, e) => handleAssetDoubleClick(a, e, virtualRow.index * columns + idx)
                "
                @context-menu="
                  (a, e) => void handleAssetContextMenu(a, e, virtualRow.index * columns + idx)
                "
                @drag-start="(a, e) => handleAssetDragStart(a, e)"
              />

              <div v-else class="skeleton-card w-full rounded" :style="{ aspectRatio: '1 / 1' }" />
            </template>
          </div>
        </div>
      </div>
    </div>

    <GridTimelineRailBridge
      :scroll-container="scrollContainerRef"
      :container-height="containerHeight"
      :virtualizer="gridVirtualizer.virtualizer.value"
      :markers="railMarkers"
      :labels="railLabels"
    />
  </div>

  <ScrollArea v-else ref="scrollAreaRef" class="mr-1 h-full">
    <div class="px-6">
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
            class="grid justify-items-center gap-4"
            :style="{
              gridTemplateColumns: `repeat(${columns}, 1fr)`,
            }"
          >
            <template
              v-for="(asset, idx) in virtualRow.assets"
              :key="asset?.id ?? `placeholder-${virtualRow.index}-${idx}`"
            >
              <AssetCard
                v-if="asset !== null"
                :asset="asset"
                :is-selected="gallerySelection.isAssetSelected(asset.id)"
                @click="(a, e) => handleAssetClick(a, e, virtualRow.index * columns + idx)"
                @double-click="
                  (a, e) => handleAssetDoubleClick(a, e, virtualRow.index * columns + idx)
                "
                @context-menu="
                  (a, e) => void handleAssetContextMenu(a, e, virtualRow.index * columns + idx)
                "
                @drag-start="(a, e) => handleAssetDragStart(a, e)"
              />

              <div v-else class="skeleton-card w-full rounded" :style="{ aspectRatio: '1 / 1' }" />
            </template>
          </div>
        </div>
      </div>
    </div>
  </ScrollArea>
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
