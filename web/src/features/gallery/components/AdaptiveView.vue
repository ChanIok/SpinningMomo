<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useElementSize } from '@vueuse/core'
import type { Asset } from '../types'
import {
  useAdaptiveVirtualizer,
  useGallerySelection,
  useGalleryLightbox,
  useTimelineRail,
} from '../composables'
import { prepareHero } from '../composables/useHeroTransition'
import { galleryApi } from '../api'
import { useGalleryStore } from '../store'
import { useI18n } from '@/composables/useI18n'
import AssetCard from './AssetCard.vue'
import GalleryAssetContextMenuContent from './GalleryAssetContextMenuContent.vue'
import GalleryScrollbarRail from './GalleryScrollbarRail.vue'
import { ContextMenu, ContextMenuContent, ContextMenuTrigger } from '@/components/ui/context-menu'

const store = useGalleryStore()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()
const { locale } = useI18n()

const scrollContainerRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)

// AdaptiveView 不再依赖 ScrollArea，避免第三方滚动容器内部测量语义干扰 thumb 尺寸。
const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)

const adaptiveVirtualizer = useAdaptiveVirtualizer({
  containerRef: scrollContainerRef,
  containerWidth,
})

const { markers: railMarkers, labels: railLabels } = useTimelineRail({
  isTimelineMode: computed(() => store.isTimelineMode),
  buckets: computed(() => store.timelineBuckets),
  locale,
  getOffsetByAssetIndex(assetIndex) {
    const rowIndex = adaptiveVirtualizer.rowIndexByAssetIndex.value.get(assetIndex)
    if (rowIndex === undefined) {
      return undefined
    }

    return adaptiveVirtualizer.rows.value[rowIndex]?.start
  },
})

onMounted(async () => {
  await adaptiveVirtualizer.init()
})

function handleScroll(event: Event) {
  // 轨道指示器与 hover 映射都依赖真实 scrollTop，因此这里直接从原生容器同步。
  const target = event.target as HTMLElement
  scrollTop.value = target.scrollTop
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

function handleAssetContextMenu(asset: Asset, event: MouseEvent, index: number) {
  void gallerySelection.handleAssetContextMenu(asset, event, index)
}

function scrollToIndex(index: number) {
  adaptiveVirtualizer.scrollToIndex(index)
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

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div class="flex h-full">
    <div
      ref="scrollContainerRef"
      class="hide-scrollbar flex-1 overflow-auto py-2 pr-2 pl-6"
      @scroll="handleScroll"
    >
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
              transform: `translateY(${virtualRow.start}px)`,
              display: 'flex',
              gap: `${adaptiveVirtualizer.gap}px`,
            }"
          >
            <template v-for="item in virtualRow.items" :key="item.id">
              <div
                :data-index="item.index"
                class="shrink-0"
                :style="{ width: `${item.width}px`, height: `${item.height}px` }"
              >
                <ContextMenu v-if="item.asset !== null">
                  <ContextMenuTrigger as-child>
                    <AssetCard
                      :asset="item.asset"
                      :aspect-ratio="`${item.width} / ${item.height}`"
                      :is-selected="gallerySelection.isAssetSelected(item.asset.id)"
                      @click="(asset, event) => handleAssetClick(asset, event, item.index)"
                      @double-click="
                        (asset, event) => handleAssetDoubleClick(asset, event, item.index)
                      "
                      @context-menu="
                        (asset, event) => handleAssetContextMenu(asset, event, item.index)
                      "
                    />
                  </ContextMenuTrigger>
                  <ContextMenuContent>
                    <GalleryAssetContextMenuContent />
                  </ContextMenuContent>
                </ContextMenu>

                <div v-else class="h-full w-full animate-pulse rounded-lg bg-muted" />
              </div>
            </template>
          </div>
        </div>
      </div>
    </div>

    <GalleryScrollbarRail
      :container-height="containerHeight"
      :scroll-top="scrollTop"
      :viewport-height="containerHeight"
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
