<script setup lang="ts">
import { ref, computed, onMounted, watch } from 'vue'
import { useElementSize } from '@vueuse/core'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'
import {
  useGalleryView,
  useGallerySelection,
  useGalleryLightbox,
  useGridVirtualizer,
} from '../composables'
import AssetCard from './AssetCard.vue'
import GalleryAssetContextMenuContent from './GalleryAssetContextMenuContent.vue'
import TimelineScrollbar from './TimelineScrollbar.vue'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'
import { ContextMenu, ContextMenuContent, ContextMenuTrigger } from '@/components/ui/context-menu'

const store = useGalleryStore()
const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const scrollContainerRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)
const viewportHeight = ref(0)

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

function handleScroll(event: Event) {
  const target = event.target as HTMLElement
  scrollTop.value = target.scrollTop
  viewportHeight.value = target.clientHeight
}

function handleAssetClick(asset: Asset, event: MouseEvent, index: number) {
  void gallerySelection.handleAssetClick(asset, event, index)
}

function handleAssetDoubleClick(asset: Asset, event: MouseEvent, index: number) {
  gallerySelection.handleAssetDoubleClick(asset, event)
  void galleryLightbox.openLightbox(index)
}

function handleAssetContextMenu(asset: Asset, event: MouseEvent, index: number) {
  void gallerySelection.handleAssetContextMenu(asset, event, index)
}
</script>

<template>
  <div v-if="isTimelineMode" class="flex h-full">
    <div
      ref="scrollContainerRef"
      class="hide-scrollbar flex-1 overflow-auto pr-2 pl-6"
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
            class="grid justify-items-center gap-4"
            :style="{
              gridTemplateColumns: `repeat(${columns}, 1fr)`,
            }"
          >
            <template
              v-for="(asset, idx) in virtualRow.assets"
              :key="asset?.id ?? `placeholder-${virtualRow.index}-${idx}`"
            >
              <ContextMenu v-if="asset !== null">
                <ContextMenuTrigger as-child>
                  <AssetCard
                    :asset="asset"
                    :is-selected="gallerySelection.isAssetSelected(asset.id)"
                    @click="(a, e) => handleAssetClick(a, e, virtualRow.index * columns + idx)"
                    @double-click="
                      (a, e) => handleAssetDoubleClick(a, e, virtualRow.index * columns + idx)
                    "
                    @context-menu="
                      (a, e) => handleAssetContextMenu(a, e, virtualRow.index * columns + idx)
                    "
                  />
                </ContextMenuTrigger>
                <ContextMenuContent>
                  <GalleryAssetContextMenuContent />
                </ContextMenuContent>
              </ContextMenu>

              <div
                v-else
                class="skeleton-card rounded-lg"
                :style="{
                  width: `${galleryView.viewSize.value}px`,
                  height: `${galleryView.viewSize.value}px`,
                }"
              />
            </template>
          </div>
        </div>
      </div>
    </div>

    <TimelineScrollbar
      v-if="store.timelineBuckets.length > 0"
      :buckets="store.timelineBuckets"
      :container-height="containerHeight"
      :scroll-top="scrollTop"
      :viewport-height="viewportHeight"
      :estimated-row-height="gridVirtualizer.estimatedRowHeight.value"
      :columns="columns"
      :virtualizer="gridVirtualizer.virtualizer.value"
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
              <ContextMenu v-if="asset !== null">
                <ContextMenuTrigger as-child>
                  <AssetCard
                    :asset="asset"
                    :is-selected="gallerySelection.isAssetSelected(asset.id)"
                    @click="(a, e) => handleAssetClick(a, e, virtualRow.index * columns + idx)"
                    @double-click="
                      (a, e) => handleAssetDoubleClick(a, e, virtualRow.index * columns + idx)
                    "
                    @context-menu="
                      (a, e) => handleAssetContextMenu(a, e, virtualRow.index * columns + idx)
                    "
                  />
                </ContextMenuTrigger>
                <ContextMenuContent>
                  <GalleryAssetContextMenuContent />
                </ContextMenuContent>
              </ContextMenu>

              <div
                v-else
                class="skeleton-card rounded-lg"
                :style="{
                  width: `${galleryView.viewSize.value}px`,
                  height: `${galleryView.viewSize.value}px`,
                }"
              />
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
