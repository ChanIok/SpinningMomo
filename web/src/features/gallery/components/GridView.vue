<script setup lang="ts">
import { ref, computed, onMounted, watch } from 'vue'
import { useGalleryStore } from '../store'
import {
  useGalleryView,
  useGallerySelection,
  useGalleryLightbox,
  useGridVirtualizer,
} from '../composables'
import { useElementSize } from '@vueuse/core'
import AssetCard from './AssetCard.vue'
import TimelineScrollbar from './TimelineScrollbar.vue'
import ScrollArea from '@/components/ui/scroll-area/ScrollArea.vue'

// Store 和 Composables
const store = useGalleryStore()
const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryLightbox = useGalleryLightbox()

// 响应式变量和引用
const scrollAreaRef = ref<InstanceType<typeof ScrollArea> | null>(null)
const scrollContainerRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)
const viewportHeight = ref(0)

const isTimelineMode = computed(() => store.isTimelineMode)
const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)
const columns = computed(() => {
  const itemSize = galleryView.viewSize.value
  const gap = 16 // gap-4 对应 16px (1rem)
  console.log('containerWidth', containerWidth.value, itemSize, gap)
  return Math.max(1, Math.floor((containerWidth.value + gap) / (itemSize + gap)))
})

// 统一使用虚拟列表 composable
const gridVirtualizer = useGridVirtualizer({
  containerRef: scrollContainerRef,
  columns: columns,
  containerWidth: containerWidth,
})

// 监听模式切换，重新初始化
watch(isTimelineMode, async (newValue) => {
  // 切换到普通模式时，确保 scrollContainerRef 指向 ScrollArea 的 viewport
  setTimeout(async () => {
    if (!newValue && scrollAreaRef.value) {
      scrollContainerRef.value = scrollAreaRef.value.viewportElement
    }
    console.log('scrollAreaRef', scrollAreaRef.value)
    await gridVirtualizer.init()
  }, 1000)
})

// 生命周期钩子
onMounted(async () => {
  // 对于普通模式，从 ScrollArea 获取内部的 viewport ref
  if (!isTimelineMode.value && scrollAreaRef.value) {
    scrollContainerRef.value = scrollAreaRef.value.viewportElement
  }

  await gridVirtualizer.init()
})

// 事件处理函数
function handleScroll(event: Event) {
  const target = event.target as HTMLElement
  scrollTop.value = target.scrollTop
  viewportHeight.value = target.clientHeight
}

function handleAssetClick(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetClick(asset, event, [])
}

function handleAssetDoubleClick(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetDoubleClick(asset, event)
  galleryLightbox.openLightbox(asset)
}

function handleAssetContextMenu(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetContextMenu(asset, event)
}

function handleAssetPreview(asset: any) {
  galleryLightbox.openLightbox(asset)
}
</script>

<template>
  <!-- 时间线模式 -->
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
              <!-- 已加载的资产 -->
              <AssetCard
                v-if="asset !== null"
                :asset="asset"
                :is-selected="gallerySelection.isAssetSelected(asset.id)"
                :is-active="gallerySelection.isAssetActive(asset.id)"
                :show-name="galleryView.viewSize.value >= 256"
                :show-size="galleryView.viewSize.value >= 256"
                @click="handleAssetClick"
                @double-click="handleAssetDoubleClick"
                @context-menu="handleAssetContextMenu"
                @preview="handleAssetPreview"
              />

              <!-- 骨架屏占位 -->
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

    <!-- 时间线滚动条 -->
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

  <!-- 普通网格模式（虚拟列表） -->
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
              <!-- 已加载的资产 -->
              <AssetCard
                v-if="asset !== null"
                :asset="asset"
                :is-selected="gallerySelection.isAssetSelected(asset.id)"
                :is-active="gallerySelection.isAssetActive(asset.id)"
                :show-name="galleryView.viewSize.value >= 256"
                :show-size="galleryView.viewSize.value >= 256"
                @click="handleAssetClick"
                @double-click="handleAssetDoubleClick"
                @context-menu="handleAssetContextMenu"
                @preview="handleAssetPreview"
              />

              <!-- 骨架屏占位 -->
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
/* 隐藏滚动条 - Webkit 浏览器 (Chrome, Safari, Edge) */
.hide-scrollbar::-webkit-scrollbar {
  display: none;
}

/* 隐藏滚动条 - Firefox */
.hide-scrollbar {
  scrollbar-width: none;
}

/* 骨架屏动画 */
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
