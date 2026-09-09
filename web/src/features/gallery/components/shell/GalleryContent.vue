<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useGalleryStore } from '../../store'
import type { ViewMode } from '../../types'
import GridView from '../viewer/GridView.vue'
import ListView from '../viewer/ListView.vue'
import MasonryView from '../viewer/MasonryView.vue'
import AdaptiveView from '../viewer/AdaptiveView.vue'
import GallerySharedContextMenu from '../menus/GallerySharedContextMenu.vue'
import GalleryMoveToFolderDialog from '../dialogs/GalleryMoveToFolderDialog.vue'
import GalleryDeleteAssetsDialog from '../dialogs/GalleryDeleteAssetsDialog.vue'
import GalleryEmptyState from './GalleryEmptyState.vue'

const props = withDefaults(
  defineProps<{
    toolbarHeight?: number
  }>(),
  {
    toolbarHeight: 0,
  }
)

const store = useGalleryStore()

const isAllMediaSelected = computed(
  () => !store.filter.folderId && (!store.filter.tagIds || store.filter.tagIds.length === 0)
)

const showEmptyState = computed(
  () =>
    isAllMediaSelected.value &&
    store.hasInitialQueried &&
    store.totalCount === 0 &&
    store.queryStatus !== 'loading'
)

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

// 每次切换从旧视图捕获锚点，保留到新视图挂载后读取。
const activeAnchorIndex = ref<number | undefined>(undefined)

function getTopAssetIndex(mode: ViewMode): number {
  if (mode === 'grid') return gridViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
  if (mode === 'list') return listViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
  if (mode === 'masonry') return masonryViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
  return adaptiveViewRef.value?.getTopVisibleAssetIndex?.() ?? 0
}

// 默认 pre 调度在旧视图卸载前执行，并合并同一轮内的连续模式变更。
watch(viewMode, (_mode, previousMode) => {
  const topIndex = getTopAssetIndex(previousMode)
  activeAnchorIndex.value = topIndex >= 0 && topIndex < store.totalCount ? topIndex : undefined
})

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
  <div class="gallery-layout-scene h-full w-full">
    <GalleryEmptyState v-if="showEmptyState" />
    <GridView
      v-else-if="viewMode === 'grid'"
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
