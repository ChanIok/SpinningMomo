import { computed, watch, shallowRef, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import type { Asset, DateGrouping, TimelineBucket } from '../types'
import {
  buildTimelineSections,
  getTimelineHeaderDescriptors,
  getTimelineHeaderHeight,
  hasCompleteTimelineBuckets,
  type TimelineHeaderDescriptor,
  type TimelineHeaderKind,
} from './timelineLayout'

export interface UseGridVirtualizerOptions {
  containerRef: Ref<HTMLElement | null>
  columns: Ref<number>
  containerWidth: Ref<number>
  /** 滚动容器顶部到虚拟列表起点的真实距离。 */
  scrollMargin: Ref<number>
  gap: Ref<number>
}

type GridRowKind = 'assets' | TimelineHeaderKind

interface GridLayoutRow {
  index: number
  kind: GridRowKind
  start: number
  size: number
  assetStartIndex?: number
  assetCount?: number
  date?: string
  month?: string
  count?: number
  startIndex?: number
  endIndex?: number
}

type GridLayoutMode = 'flat' | 'timeline'

interface GridTimelineSection {
  startIndex: number
  endIndex: number
  assetRowStartIndex: number
}

interface GridLayout {
  mode: GridLayoutMode
  totalRows: number
  // 只有时间线分组模式需要保存完整视觉行；flat 模式按需生成当前虚拟行。
  rows: GridLayoutRow[]
  sections: GridTimelineSection[]
}

export interface VirtualRow {
  /** 虚拟列表中的视觉行索引，包含标题行。 */
  index: number
  kind: GridRowKind
  assets: (Asset | null)[]
  assetStartIndex?: number
  date?: string
  month?: string
  count?: number
  startIndex?: number
  endIndex?: number
  start: number
  size: number
}

function buildFlatGridLayout(total: number, columns: number): GridLayout {
  const safeColumns = Math.max(1, columns)

  return {
    mode: 'flat',
    totalRows: Math.ceil(Math.max(0, total) / safeColumns),
    rows: [],
    sections: [],
  }
}

function buildTimelineGridLayout(
  buckets: TimelineBucket[],
  total: number,
  columns: number,
  assetRowSize: number,
  gap: number,
  compact: boolean,
  grouping: DateGrouping
): GridLayout {
  const rows: GridLayoutRow[] = []
  const gridSections: GridTimelineSection[] = []
  const safeColumns = Math.max(1, columns)
  const sections = buildTimelineSections(buckets, grouping, total)
  let contentStart = 0

  function appendHeader(header: TimelineHeaderDescriptor) {
    const contentHeight = getTimelineHeaderHeight(compact)
    rows.push({
      index: rows.length,
      kind: header.kind,
      start: contentStart,
      size: contentHeight + gap,
      date: header.date,
      month: header.month,
      count: header.count,
      startIndex: header.startIndex,
      endIndex: header.endIndex,
    })
    contentStart += contentHeight + gap
  }

  for (const section of sections) {
    for (const header of getTimelineHeaderDescriptors(section, grouping)) {
      appendHeader(header)
    }

    const assetRowStartIndex = rows.length
    for (
      let assetStartIndex = section.startIndex;
      assetStartIndex < section.endIndex;
      assetStartIndex += safeColumns
    ) {
      const assetCount = Math.min(safeColumns, section.endIndex - assetStartIndex)
      const rowIndex = rows.length
      rows.push({
        index: rowIndex,
        kind: 'assets',
        start: contentStart,
        size: assetRowSize,
        assetStartIndex,
        assetCount,
      })
      contentStart += assetRowSize
    }

    gridSections.push({
      startIndex: section.startIndex,
      endIndex: section.endIndex,
      assetRowStartIndex,
    })
  }

  return { mode: 'timeline', totalRows: rows.length, rows, sections: gridSections }
}

function getTimelineAssetRowIndex(
  sections: GridTimelineSection[],
  index: number,
  columns: number
): number | undefined {
  const safeColumns = Math.max(1, columns)
  let left = 0
  let right = sections.length - 1

  while (left <= right) {
    const middle = Math.floor((left + right) / 2)
    const section = sections[middle]
    if (!section) {
      return undefined
    }

    if (index < section.startIndex) {
      right = middle - 1
      continue
    }

    if (index >= section.endIndex) {
      left = middle + 1
      continue
    }

    return section.assetRowStartIndex + Math.floor((index - section.startIndex) / safeColumns)
  }

  return undefined
}

function getAssetRowIndex(
  layout: GridLayout,
  index: number,
  total: number,
  columns: number
): number | undefined {
  if (!Number.isInteger(index) || index < 0 || index >= total) {
    return undefined
  }

  if (layout.mode === 'flat') {
    return Math.floor(index / Math.max(1, columns))
  }

  return getTimelineAssetRowIndex(layout.sections, index, columns)
}

function getGridRow(
  layout: GridLayout,
  rowIndex: number,
  total: number,
  columns: number,
  rowSize: number
): GridLayoutRow | undefined {
  if (rowIndex < 0 || rowIndex >= layout.totalRows) {
    return undefined
  }

  if (layout.mode === 'timeline') {
    return layout.rows[rowIndex]
  }

  const safeColumns = Math.max(1, columns)
  const assetStartIndex = rowIndex * safeColumns
  return {
    index: rowIndex,
    kind: 'assets',
    start: rowIndex * rowSize,
    size: rowSize,
    assetStartIndex,
    assetCount: Math.min(safeColumns, total - assetStartIndex),
  }
}

export function useGridVirtualizer(options: UseGridVirtualizerOptions) {
  const { containerRef, columns, containerWidth, scrollMargin, gap } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const isTimelineMode = computed(() => store.isTimelineMode)
  const totalCount = computed(() =>
    isTimelineMode.value ? store.timelineTotalCount : store.totalCount
  )

  const estimatedRowHeight = computed(() => {
    const width = containerWidth.value || containerRef.value?.clientWidth || 0
    if (width === 0) return 200

    const cardWidth = Math.max(
      1,
      (width - (columns.value - 1) * gap.value) / Math.max(columns.value, 1)
    )
    return cardWidth + gap.value
  })

  const hasTimelineLayout = computed(
    () =>
      store.isDateGroupingEnabled &&
      store.timelineBuckets.length > 0 &&
      hasCompleteTimelineBuckets(store.timelineBuckets, totalCount.value)
  )

  const layout = computed<GridLayout>(() => {
    if (hasTimelineLayout.value) {
      return buildTimelineGridLayout(
        store.timelineBuckets,
        totalCount.value,
        columns.value,
        estimatedRowHeight.value,
        gap.value,
        store.isCompactWindow,
        store.view.dateGrouping
      )
    }

    return buildFlatGridLayout(totalCount.value, columns.value)
  })

  const totalRows = computed(() => layout.value.totalRows)

  const virtualizer = useVirtualizer({
    get count() {
      return totalRows.value
    },
    getScrollElement: () => containerRef.value,
    estimateSize: (index) => layout.value.rows[index]?.size ?? estimatedRowHeight.value,
    get scrollMargin() {
      return scrollMargin.value
    },
    paddingStart: 0,
    paddingEnd: 16,
    overscan: 10,
  })

  const virtualRows = shallowRef<VirtualRow[]>([])

  function syncVirtualRows(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>,
    total: number
  ) {
    if (items.length === 0) {
      virtualRows.value = []
      store.setVisibleRange(undefined, undefined)
      return
    }

    const currentLayout = layout.value
    let visibleStartIndex = total
    let visibleEndIndex = -1
    items.forEach((item) => {
      const row = getGridRow(
        currentLayout,
        item.index,
        total,
        columns.value,
        estimatedRowHeight.value
      )
      if (!row || row.kind !== 'assets' || row.assetStartIndex === undefined) {
        return
      }

      visibleStartIndex = Math.min(visibleStartIndex, row.assetStartIndex)
      visibleEndIndex = Math.max(
        visibleEndIndex,
        Math.min(total - 1, row.assetStartIndex + (row.assetCount ?? 0) - 1)
      )
    })

    if (visibleEndIndex < visibleStartIndex) {
      store.setVisibleRange(undefined, undefined)
    } else {
      store.setVisibleRange(Math.max(0, visibleStartIndex), Math.min(total - 1, visibleEndIndex))
    }

    virtualRows.value = items.flatMap((virtualItem): VirtualRow[] => {
      const row = getGridRow(
        currentLayout,
        virtualItem.index,
        total,
        columns.value,
        estimatedRowHeight.value
      )
      if (!row) {
        return []
      }

      if (row.kind !== 'assets' || row.assetStartIndex === undefined) {
        return [
          {
            index: row.index,
            kind: row.kind,
            assets: [],
            date: row.date,
            month: row.month,
            count: row.count,
            startIndex: row.startIndex,
            endIndex: row.endIndex,
            start: Math.round(virtualItem.start),
            size: Math.round(virtualItem.size),
          },
        ]
      }

      const endIndex = Math.min(
        row.assetStartIndex + (row.assetCount ?? 0) - 1,
        Math.max(0, total - 1)
      )
      return [
        {
          index: row.index,
          kind: row.kind,
          assets: store.getAssetsInRange(row.assetStartIndex, endIndex),
          assetStartIndex: row.assetStartIndex,
          start: Math.round(virtualItem.start),
          size: Math.round(virtualItem.size),
        },
      ]
    })
  }

  function loadMissingData(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>,
    total: number
  ) {
    if (items.length === 0) return

    const currentLayout = layout.value
    const visibleIndexes: number[] = []
    items.forEach((item) => {
      const row = getGridRow(
        currentLayout,
        item.index,
        total,
        columns.value,
        estimatedRowHeight.value
      )
      if (!row || row.kind !== 'assets' || row.assetStartIndex === undefined) {
        return
      }

      const assetCount = Math.min(row.assetCount ?? 0, total - row.assetStartIndex)
      for (let offset = 0; offset < assetCount; offset += 1) {
        visibleIndexes.push(row.assetStartIndex + offset)
      }
    })

    void galleryData.ensureIndexesLoaded(visibleIndexes)
  }

  watch(
    () => ({
      items: virtualizer.value.getVirtualItems(),
      columns: columns.value,
      estimatedRowHeight: estimatedRowHeight.value,
      totalCount: totalCount.value,
      layout: layout.value,
      paginatedAssetsVersion: store.paginatedAssetsVersion,
    }),
    ({ items, totalCount: total }) => {
      syncVirtualRows(items, total)
      void loadMissingData(items, total)
    }
  )

  function getAssetOffset(index: number): number | undefined {
    const currentLayout = layout.value
    const rowIndex = getAssetRowIndex(currentLayout, index, totalCount.value, columns.value)
    if (rowIndex === undefined) {
      return undefined
    }

    return getGridRow(
      currentLayout,
      rowIndex,
      totalCount.value,
      columns.value,
      estimatedRowHeight.value
    )?.start
  }

  function scrollToIndex(index: number, align: 'auto' | 'start' = 'auto') {
    const rowIndex = getAssetRowIndex(layout.value, index, totalCount.value, columns.value)
    if (rowIndex === undefined) {
      return
    }

    virtualizer.value.scrollToIndex(rowIndex, { align })
  }

  watch([layout, estimatedRowHeight], () => {
    virtualizer.value.measure()
  })

  return {
    virtualizer,
    virtualRows,
    totalRows,
    estimatedRowHeight,
    getAssetOffset,
    scrollToIndex,
  }
}
