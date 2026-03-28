<script setup lang="ts">
import { computed, onMounted, ref, type ComponentPublicInstance } from 'vue'
import { useElementSize } from '@vueuse/core'
import type { Asset } from '../types'
import {
  useGalleryView,
  useGallerySelection,
  useGalleryLightbox,
  useMasonryVirtualizer,
} from '../composables'
import { prepareHero } from '../composables/useHeroTransition'
import { galleryApi } from '../api'
import AssetCard from './AssetCard.vue'
import GalleryAssetContextMenuContent from './GalleryAssetContextMenuContent.vue'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'
import ScrollBar from '@/components/ui/scroll-area/ScrollBar.vue'
import { ContextMenu, ContextMenuContent, ContextMenuTrigger } from '@/components/ui/context-menu'

const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const scrollContainerRef = ref<HTMLElement | null>(null)

const { width: containerWidth } = useElementSize(scrollContainerRef)
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

onMounted(async () => {
  // ScrollArea 的真实滚动容器在挂载后才可通过 viewportElement 获取
  if (scrollAreaRef.value) {
    scrollContainerRef.value = scrollAreaRef.value.viewportElement
  }

  await masonryVirtualizer.init()
})

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

function handleAssetContextMenu(asset: Asset, event: MouseEvent, index: number) {
  void gallerySelection.handleAssetContextMenu(asset, event, index)
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
  <ScrollArea ref="scrollAreaRef" type="always" class="mr-1 h-full">
    <template #scrollbar>
      <ScrollBar
        class="w-4 p-0.5"
        thumb-class="bg-muted-foreground/35 hover:bg-muted-foreground/50"
      />
    </template>
    <div class="px-6">
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
          <ContextMenu v-if="virtualItem.asset !== null">
            <ContextMenuTrigger as-child>
              <AssetCard
                :asset="virtualItem.asset"
                :aspect-ratio="getAssetAspectRatio(virtualItem.asset)"
                :is-selected="gallerySelection.isAssetSelected(virtualItem.asset.id)"
                @click="(asset, event) => handleAssetClick(asset, event, virtualItem.index)"
                @double-click="
                  (asset, event) => handleAssetDoubleClick(asset, event, virtualItem.index)
                "
                @context-menu="
                  (asset, event) => handleAssetContextMenu(asset, event, virtualItem.index)
                "
              />
            </ContextMenuTrigger>
            <ContextMenuContent>
              <GalleryAssetContextMenuContent />
            </ContextMenuContent>
          </ContextMenu>

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
  </ScrollArea>
</template>
