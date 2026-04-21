<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRoute } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import { useMapStore } from '@/features/map/store'

const route = useRoute()
const galleryStore = useGalleryStore()
const mapStore = useMapStore()
const isCollapsed = ref(true)

const debugState = computed(() => {
  const loadedPages = [...galleryStore.paginatedAssets.keys()].sort((left, right) => left - right)

  return {
    route: String(route.name ?? ''),
    queryVersion: galleryStore.queryVersion,
    isRefreshing: galleryStore.isRefreshing,
    isLoading: galleryStore.isLoading,
    totalCount: galleryStore.totalCount,
    currentPage: galleryStore.currentPage,
    perPage: galleryStore.perPage,
    visibleRange: `${galleryStore.visibleRange.startIndex ?? '-'} ~ ${galleryStore.visibleRange.endIndex ?? '-'}`,
    loadedPages: loadedPages.length > 0 ? loadedPages.join(', ') : '-',
    activeAssetId: galleryStore.selection.activeAssetId ?? '-',
    activeIndex: galleryStore.selection.activeIndex ?? '-',
    selectedCount: galleryStore.selection.selectedIds.size,
    anchorIndex: galleryStore.selection.anchorIndex ?? '-',
    lightboxOpen: galleryStore.lightbox.isOpen,
    lightboxClosing: galleryStore.lightbox.isClosing,
    detailsFocus: galleryStore.detailsPanel.type,
    sort: `${galleryStore.sortBy} / ${galleryStore.sortOrder}`,
    includeSubfolders: galleryStore.includeSubfolders,
    filterFolderId: galleryStore.filter.folderId ?? '-',
    filterType: galleryStore.filter.type ?? '-',
    filterSearch: galleryStore.filter.searchQuery?.trim() || '-',
    timelineBuckets: galleryStore.timelineBuckets.length,
    mapMarkerCount: mapStore.markers.length,
    mapWorldId: mapStore.runtimeOptions.currentWorldId?.trim() || '-',
  }
})
</script>

<template>
  <div
    class="absolute bottom-2 left-2 z-[9900] max-w-[420px] rounded-md border border-white/15 bg-black/75 px-3 py-2 text-[11px] leading-4 text-white shadow-lg backdrop-blur-sm"
  >
    <div class="mb-1 flex items-center justify-between gap-2">
      <div class="font-semibold tracking-wide text-white/90">Gallery Debug</div>
      <div class="flex items-center gap-1">
        <button
          type="button"
          class="h-5 rounded border border-white/25 px-1.5 text-[10px] leading-none text-white/85 transition hover:bg-white/15"
          @click="isCollapsed = !isCollapsed"
        >
          {{ isCollapsed ? '展开' : '收起' }}
        </button>
      </div>
    </div>
    <div v-if="!isCollapsed">
      <div>route: {{ debugState.route }}</div>
      <div>
        query: v{{ debugState.queryVersion }} / refreshing={{ debugState.isRefreshing }} /
        loading={{ debugState.isLoading }}
      </div>
      <div>
        page: {{ debugState.currentPage }} / total={{ debugState.totalCount }} / per={{
          debugState.perPage
        }}
      </div>
      <div>visibleRange: {{ debugState.visibleRange }}</div>
      <div>loadedPages: {{ debugState.loadedPages }}</div>
      <div>active: id={{ debugState.activeAssetId }} / index={{ debugState.activeIndex }}</div>
      <div>selected={{ debugState.selectedCount }} / anchor={{ debugState.anchorIndex }}</div>
      <div>
        lightbox: open={{ debugState.lightboxOpen }} / closing={{ debugState.lightboxClosing }}
      </div>
      <div>detailsFocus: {{ debugState.detailsFocus }}</div>
      <div>sort: {{ debugState.sort }}</div>
      <div>includeSubfolders: {{ debugState.includeSubfolders }}</div>
      <div>filter: folder={{ debugState.filterFolderId }} / type={{ debugState.filterType }}</div>
      <div>search: {{ debugState.filterSearch }}</div>
      <div>timelineBuckets: {{ debugState.timelineBuckets }}</div>
      <div>map: markers={{ debugState.mapMarkerCount }} / worldId={{ debugState.mapWorldId }}</div>
    </div>
  </div>
</template>
