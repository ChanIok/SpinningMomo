import { computed, watch, ref, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import type { Asset } from '../types'

export interface UseGridVirtualizerOptions {
  containerRef: Ref<HTMLElement | null>
  columns: Ref<number>
  containerWidth: Ref<number>
}

export interface VirtualRow {
  index: number
  assets: (Asset | null)[]
  start: number
  size: number
}

export function useGridVirtualizer(options: UseGridVirtualizerOptions) {
  const { containerRef, columns, containerWidth } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const isTimelineMode = computed(() => store.isTimelineMode && store.viewConfig.mode === 'grid')
  const totalCount = computed(() =>
    isTimelineMode.value ? store.timelineTotalCount : store.totalCount
  )

  const totalRows = computed(() => Math.ceil(totalCount.value / columns.value))

  const estimatedRowHeight = computed(() => {
    const width = containerWidth.value || containerRef.value?.clientWidth || 0
    if (width === 0) return 200

    const gap = 16
    const cardWidth = Math.floor((width - (columns.value - 1) * gap) / columns.value)
    return cardWidth + gap
  })

  const virtualizer = useVirtualizer({
    get count() {
      return totalRows.value
    },
    getScrollElement: () => containerRef.value,
    estimateSize: () => estimatedRowHeight.value,
    paddingStart: 0,
    paddingEnd: 16,
    overscan: 10,
  })

  const virtualRows = ref<VirtualRow[]>([])
  const loadingPages = ref<Set<number>>(new Set())

  function syncVirtualRows(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>,
    cols: number,
    total: number
  ) {
    if (items.length === 0) {
      virtualRows.value = []
      store.setVisibleRange(undefined, undefined)
      return
    }

    const firstVisibleRow = items[0]!
    const lastVisibleRow = items[items.length - 1]!
    store.setVisibleRange(
      Math.max(0, firstVisibleRow.index * cols),
      Math.min(total - 1, (lastVisibleRow.index + 1) * cols - 1)
    )

    virtualRows.value = items.map((virtualRow) => {
      const startIndex = virtualRow.index * cols
      const endIndex = Math.min(startIndex + cols - 1, total - 1)
      return {
        index: virtualRow.index,
        assets: store.getAssetsInRange(startIndex, endIndex),
        start: virtualRow.start,
        size: virtualRow.size,
      }
    })
  }

  async function loadMissingData(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>,
    cols: number,
    total: number
  ): Promise<void> {
    if (items.length === 0) return

    const visibleIndexes: number[] = []
    items.forEach((item) => {
      const start = item.index * cols
      const end = Math.min(start + cols, total)
      for (let i = start; i < end; i++) visibleIndexes.push(i)
    })

    const neededPages = new Set(visibleIndexes.map((idx) => Math.floor(idx / store.perPage) + 1))
    const loadPromises: Promise<void>[] = []

    neededPages.forEach((pageNum) => {
      if (!store.isPageLoaded(pageNum) && !loadingPages.value.has(pageNum)) {
        loadingPages.value.add(pageNum)
        const loadPromise = galleryData.loadPage(pageNum).finally(() => {
          loadingPages.value.delete(pageNum)
        })
        loadPromises.push(loadPromise)
      }
    })

    if (loadPromises.length > 0) {
      await Promise.all(loadPromises)
    }
  }

  watch(
    () => ({
      items: virtualizer.value.getVirtualItems(),
      columns: columns.value,
      totalCount: totalCount.value,
      paginatedAssetsVersion: store.paginatedAssetsVersion,
    }),
    async ({ items, columns: cols, totalCount: total }) => {
      syncVirtualRows(items, cols, total)
      await loadMissingData(items, cols, total)
      syncVirtualRows(virtualizer.value.getVirtualItems(), columns.value, totalCount.value)
    }
  )

  async function init() {
    if (isTimelineMode.value) {
      await galleryData.loadTimelineData()
    } else {
      await galleryData.loadAllAssets()
    }
  }

  watch(estimatedRowHeight, () => {
    if (virtualRows.value.length > 0) virtualizer.value.measure()
  })

  watch(columns, () => {
    if (virtualRows.value.length > 0) virtualizer.value.measure()
  })

  return {
    virtualizer,
    virtualRows,
    totalRows,
    estimatedRowHeight,
    init,
  }
}
