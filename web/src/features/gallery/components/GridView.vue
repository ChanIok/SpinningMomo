<script setup lang="ts">
import { ref, computed, onMounted, watch } from 'vue'
import {
  useGalleryView,
  useGallerySelection,
  useGalleryData,
  useTimeline,
  useGalleryLightbox,
} from '../composables'
import { useElementSize } from '@vueuse/core'
import AssetCard from './AssetCard.vue'
import TimelineScrollbar from './TimelineScrollbar.vue'

// Composables
const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()
const galleryData = useGalleryData()
const galleryLightbox = useGalleryLightbox()

// 响应式变量和引用
const scrollContainerRef = ref<HTMLElement | null>(null)
const scrollTop = ref(0)
const viewportHeight = ref(0)

// 计算属性
const isTimelineMode = computed(() => galleryData.isTimelineMode.value)
const { width: containerWidth, height: containerHeight } = useElementSize(scrollContainerRef)
const columns = computed(() => {
  const itemSize = galleryView.viewSize.value
  const gap = 16 // gap-4 对应 16px (1rem)
  return Math.max(1, Math.floor((containerWidth.value + gap) / (itemSize + gap)))
})

// 时间线 composable
const timeline = useTimeline({
  columns: columns,
  containerRef: scrollContainerRef,
  containerWidth: containerWidth,
})

// 监听可见项变化，加载对应月份
watch(
  () => timeline.virtualizer.value.getVirtualItems(),
  () => {
    if (isTimelineMode.value) {
      // 传入 galleryData 的加载月份方法
      timeline.checkAndLoadVisibleMonths((month) => galleryData.loadMonthAssets(month))
    }
  },
  { deep: true }
)

// 监听时间线模式切换
watch(isTimelineMode, (newValue) => {
  if (newValue && galleryData.timelineBuckets.value.length > 0) {
    timeline.init()
  }
})

// 生命周期钩子
onMounted(() => {
  if (isTimelineMode.value && galleryData.timelineBuckets.value.length > 0) {
    timeline.init()
    console.log('时间线模式初始化')
  }
})

// 事件处理函数
function handleScroll(event: Event) {
  const target = event.target as HTMLElement
  scrollTop.value = target.scrollTop
  viewportHeight.value = target.clientHeight
}

function handleAssetClick(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetClick(asset, event, galleryView.sortedAssets.value)
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
        :style="{ height: `${timeline.virtualizer.value.getTotalSize()}px`, position: 'relative' }"
      >
        <div
          v-for="virtualRow in timeline.virtualizer.value.getVirtualItems()"
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
          <!-- 资产行 -->
          <div
            class="grid justify-items-center gap-4"
            :style="{
              gridTemplateColumns: `repeat(${columns}, 1fr)`,
            }"
          >
            <AssetCard
              v-for="asset in timeline.virtualRows.value[virtualRow.index]?.assets || []"
              :key="asset.id"
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
          </div>
        </div>
      </div>
    </div>

    <!-- 时间线滚动条 -->
    <TimelineScrollbar
      v-if="timeline.buckets.value.length > 0"
      :buckets="timeline.buckets.value"
      :total-content-height="timeline.virtualizer.value.getTotalSize()"
      :container-height="containerHeight"
      :scroll-top="scrollTop"
      :viewport-height="viewportHeight"
      :estimated-row-height="timeline.estimatedRowHeight.value"
      :columns="columns"
      :on-scroll-to-offset="timeline.scrollToOffset"
    />
  </div>

  <!-- 普通网格模式 -->
  <div v-else class="h-full overflow-auto p-6">
    <div
      class="grid justify-items-center gap-4"
      :style="{
        gridTemplateColumns: `repeat(auto-fill, minmax(${galleryView.viewSize.value}px, 1fr))`,
      }"
    >
      <AssetCard
        v-for="asset in galleryView.sortedAssets.value"
        :key="asset.id"
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
    </div>
  </div>
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
</style>
