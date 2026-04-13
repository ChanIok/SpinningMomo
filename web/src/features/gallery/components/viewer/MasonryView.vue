<script setup lang="ts">
import { computed, onMounted, ref, type ComponentPublicInstance } from 'vue'
import { useElementSize, useEventListener } from '@vueuse/core'
import type { Asset } from '../../types'
import {
  useGalleryView,
  useGallerySelection,
  useGalleryLightbox,
  useGalleryContextMenu,
  useMasonryVirtualizer,
  useTimelineRail,
} from '../../composables'
import { prepareHero } from '../../composables/useHeroTransition'
import { galleryApi } from '../../api'
import { useGalleryDragPayload } from '../../composables/useGalleryDragPayload'
import { useGalleryStore } from '../../store'
import { useI18n } from '@/composables/useI18n'
import AssetCard from '../asset/AssetCard.vue'
import GalleryScrollbarRail from '../shell/GalleryScrollbarRail.vue'

const store = useGalleryStore()
const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()
const galleryContextMenu = useGalleryContextMenu()
const { prepareAssetDrag } = useGalleryDragPayload()
const { locale } = useI18n()

const scrollContainerRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)

const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)
// 根据容器宽度和卡片目标尺寸计算列数，与 GridView 的算法保持一致
const columns = computed(() => {
  const itemSize = galleryView.viewSize.value
  const gap = 16
  return Math.max(1, Math.floor((containerWidth.value + gap) / (itemSize + gap)))
})

const masonryVirtualizer = useMasonryVirtualizer({
  containerRef: scrollContainerRef,
  columns,
  containerWidth,
})

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

function handleScroll(event: Event) {
  const target = event.target as HTMLElement
  scrollTop.value = target.scrollTop
}

useEventListener(scrollContainerRef, 'scroll', handleScroll)

function getAssetAspectRatio(asset: Asset | null): string {
  if (!asset || !asset.width || !asset.height || asset.width <= 0 || asset.height <= 0) {
    return '1 / 1'
  }

  return `${asset.width} / ${asset.height}`
}

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
  galleryContextMenu.openForAsset({ asset, event, index, sourceView: 'masonry' })
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
  <div class="flex h-full">
    <div
      ref="scrollContainerRef"
      class="hide-scrollbar h-full flex-1 overflow-auto py-2 pr-2 pl-6"
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
            :key="virtualItem.index"
            :ref="measureItemElement"
            :data-index="virtualItem.index"
            :style="{
              position: 'absolute',
              top: 0,
              left: 0,
              width: `${masonryVirtualizer.columnWidth.value}px`,
              transform: `translateX(${masonryVirtualizer.getLaneOffset(virtualItem.lane)}px) translateY(${virtualItem.start}px)`,
            }"
          >
            <AssetCard
              v-if="virtualItem.asset !== null"
              :asset="virtualItem.asset"
              :aspect-ratio="getAssetAspectRatio(virtualItem.asset)"
              :is-selected="gallerySelection.isAssetSelected(virtualItem.asset.id)"
              @click="(asset, event) => handleAssetClick(asset, event, virtualItem.index)"
              @double-click="
                (asset, event) => handleAssetDoubleClick(asset, event, virtualItem.index)
              "
              @context-menu="
                (asset, event) => void handleAssetContextMenu(asset, event, virtualItem.index)
              "
              @drag-start="(asset, event) => handleAssetDragStart(asset, event)"
            />

            <div
              v-else
              class="animate-pulse rounded-lg bg-muted"
              :style="{
                width: '100%',
                height: `${masonryVirtualizer.getAssetHeight(null)}px`,
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
