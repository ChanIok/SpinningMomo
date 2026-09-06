<script setup lang="ts">
import { computed, nextTick, ref, watch } from 'vue'
import { useGalleryStore } from '../../store'
import GridView from '../viewer/GridView.vue'
import ListView from '../viewer/ListView.vue'
import MasonryView from '../viewer/MasonryView.vue'
import AdaptiveView from '../viewer/AdaptiveView.vue'
import GallerySharedContextMenu from '../menus/GallerySharedContextMenu.vue'
import GalleryMoveToFolderDialog from '../dialogs/GalleryMoveToFolderDialog.vue'
import GalleryDeleteAssetsDialog from '../dialogs/GalleryDeleteAssetsDialog.vue'

const props = withDefaults(
  defineProps<{
    toolbarHeight?: number
  }>(),
  {
    toolbarHeight: 0,
  }
)

const store = useGalleryStore()
const containerRef = ref<HTMLElement | null>(null)

const viewMode = computed(() =>
  store.isCompactWindow && store.view.mode === 'list' ? 'grid' : store.view.mode
)

interface GalleryViewExposed {
  scrollToIndex: (index: number, align?: 'auto' | 'start') => void
  getCardRect: (index: number) => DOMRect | null
  getTopVisibleAssetIndex?: () => number
}

const gridViewRef = ref<GalleryViewExposed | null>(null)
const listViewRef = ref<GalleryViewExposed | null>(null)
const masonryViewRef = ref<GalleryViewExposed | null>(null)
const adaptiveViewRef = ref<GalleryViewExposed | null>(null)

// 跟踪当前激活视图的锚点资产索引，在布局切换时传给新视图保持位置
const activeAnchorIndex = ref<number | undefined>(undefined)

function getCurrentTopAssetIndex(): number {
  if (viewMode.value === 'grid') return gridViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
  if (viewMode.value === 'list') return listViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
  if (viewMode.value === 'masonry') return masonryViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
  return adaptiveViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
}

watch(
  viewMode,
  () => {
    const topIndex = getCurrentTopAssetIndex()
    if (topIndex >= 0 && topIndex < store.totalCount) {
      activeAnchorIndex.value = topIndex
    }
    void nextTick(() => {
      activeAnchorIndex.value = undefined
    })
  },
  { flush: 'sync' }
)

function scrollToIndex(index: number, align: 'auto' | 'start' = 'auto') {
  if (viewMode.value === 'grid') gridViewRef.value?.scrollToIndex(index, align)
  else if (viewMode.value === 'list') listViewRef.value?.scrollToIndex(index, align)
  else if (viewMode.value === 'masonry') masonryViewRef.value?.scrollToIndex(index, align)
  else adaptiveViewRef.value?.scrollToIndex(index, align)
}

function getCardRect(index: number): DOMRect | null {
  if (viewMode.value === 'grid') return gridViewRef.value?.getCardRect(index) ?? null
  if (viewMode.value === 'list') return listViewRef.value?.getCardRect(index) ?? null
  if (viewMode.value === 'masonry') return masonryViewRef.value?.getCardRect(index) ?? null
  return adaptiveViewRef.value?.getCardRect(index) ?? null
}

defineExpose({ scrollToIndex, getCardRect })
</script>

<template>
  <div ref="containerRef" class="gallery-layout-scene h-full w-full">
    <GridView
      v-if="viewMode === 'grid'"
      ref="gridViewRef"
      :toolbar-height="props.toolbarHeight"
      :initial-anchor-index="activeAnchorIndex"
    />
    <ListView
      v-else-if="viewMode === 'list'"
      ref="listViewRef"
      :initial-anchor-index="activeAnchorIndex"
    />
    <MasonryView
      v-else-if="viewMode === 'masonry'"
      ref="masonryViewRef"
      :toolbar-height="props.toolbarHeight"
      :initial-anchor-index="activeAnchorIndex"
    />
    <AdaptiveView
      v-else
      ref="adaptiveViewRef"
      :toolbar-height="props.toolbarHeight"
      :initial-anchor-index="activeAnchorIndex"
    />
    <GallerySharedContextMenu />
    <GalleryMoveToFolderDialog />
    <GalleryDeleteAssetsDialog />
  </div>
</template>
