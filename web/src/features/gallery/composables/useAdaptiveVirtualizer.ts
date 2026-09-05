import { computed, shallowRef, watch, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import { useGalleryLayoutMeta } from './useGalleryLayoutMeta'
import type {
  AdaptiveLayoutRowItem,
  Asset,
  AssetLayoutMetaItem,
  DateGrouping,
  TimelineBucket,
} from '../types'
import { GALLERY_CARD_GAP } from '../constants'
import {
  buildTimelineSections,
  getTimelineHeaderDescriptors,
  getTimelineHeaderHeight,
  hasCompleteTimelineBuckets,
  type TimelineHeaderDescriptor,
  type TimelineHeaderKind,
} from './timelineLayout'

export interface UseAdaptiveVirtualizerOptions {
  // 原生滚动容器；adaptive 不再依赖 ScrollArea，而是直接读写真实滚动元素。
  containerRef: Ref<HTMLElement | null>
  // 内容区宽度，用来把“按比例排版”转换成真实行宽与行高。
  containerWidth: Ref<number>
  // 目标行高由视图层根据 Pinia 状态和当前布局上下文计算后传入。
  targetRowHeight: Ref<number>
  // 滚动容器顶部到虚拟列表起点的真实距离。
  scrollMargin: Ref<number>
  // 行内与行间间距；由视图层根据当前布局模式决定。
  gap?: number
}

type AdaptiveRowKind = 'assets' | TimelineHeaderKind

interface AdaptiveLayoutRow {
  index: number
  kind: AdaptiveRowKind
  start: number
  size: number
  date?: string
  month?: string
  count?: number
  startIndex?: number
  endIndex?: number
  items: AdaptiveLayoutRowItem[]
}

export interface VirtualAdaptiveRowItem extends AdaptiveLayoutRowItem {
  // 真实资产数据按需分页加载；未加载到时保持 null，渲染骨架占位。
  asset: Asset | null
}

export interface VirtualAdaptiveRow {
  index: number
  kind: AdaptiveRowKind
  start: number
  size: number
  date?: string
  month?: string
  count?: number
  startIndex?: number
  endIndex?: number
  items: VirtualAdaptiveRowItem[]
}

// 宽高缺失或异常时回退到安全比例，避免单张错误数据把整行布局拉坏。
function normalizeAspectRatio(item: AssetLayoutMetaItem): number {
  if (!item.width || !item.height || item.width <= 0 || item.height <= 0) {
    return 1
  }

  return Math.max(0.25, Math.min(4, item.width / item.height))
}

function buildAdaptiveRows(
  metaItems: AssetLayoutMetaItem[],
  contentWidth: number,
  targetRowHeight: number,
  gap: number,
  timelineBuckets: TimelineBucket[],
  compact: boolean,
  grouping: DateGrouping
): { rows: AdaptiveLayoutRow[]; rowIndexByAssetIndex: Map<number, number> } {
  // 这一层只做“几何排版”，不关心真实 Asset 是否已加载。
  // 输入是轻量布局元数据，输出是稳定的视觉行和 assetIndex -> rowIndex 映射。
  if (metaItems.length === 0 || contentWidth <= 0 || targetRowHeight <= 0) {
    return { rows: [], rowIndexByAssetIndex: new Map() }
  }

  const rows: AdaptiveLayoutRow[] = []
  const rowIndexByAssetIndex = new Map<number, number>()
  let currentItems: Array<{ index: number; id: number; aspectRatio: number }> = []
  let currentAspectSum = 0
  let currentStart = 0

  const finalizeRow = (justify: boolean) => {
    // justify=true 表示普通行需要铺满内容宽度；最后一行则保持目标高度，不强行拉伸。
    if (currentItems.length === 0) {
      return
    }

    const rowContentWidth = Math.max(1, Math.floor(contentWidth))
    const totalGap = Math.max(0, currentItems.length - 1) * gap
    const availableItemWidth = Math.max(1, rowContentWidth - totalGap)
    const maxRowHeight = Math.max(1, targetRowHeight)
    const fittedRowHeight = Math.max(1, availableItemWidth / currentAspectSum)
    const rowHeight = Math.max(
      1,
      Math.round(justify ? fittedRowHeight : Math.min(maxRowHeight, fittedRowHeight))
    )
    const rowIndex = rows.length
    let remainingWidth = availableItemWidth

    const items: AdaptiveLayoutRowItem[] = currentItems.map((item, itemIndex) => {
      // 普通行用最后一张图吸收取整误差，避免 flex 布局产生半像素宽度。
      const remainingItems = currentItems.length - itemIndex
      let width = Math.max(1, Math.round(item.aspectRatio * rowHeight))
      if (justify && itemIndex === currentItems.length - 1) {
        width = Math.max(1, remainingWidth)
      } else if (justify) {
        const maxWidth = Math.max(1, remainingWidth - (remainingItems - 1))
        width = Math.min(maxWidth, width)
      }
      remainingWidth -= width

      rowIndexByAssetIndex.set(item.index, rowIndex)
      return {
        index: item.index,
        id: item.id,
        width,
        height: rowHeight,
        aspectRatio: item.aspectRatio,
      }
    })

    rows.push({
      index: rowIndex,
      kind: 'assets',
      start: currentStart,
      size: rowHeight,
      items,
    })

    currentStart += rowHeight + gap
    currentItems = []
    currentAspectSum = 0
  }

  const appendHeader = (header: TimelineHeaderDescriptor) => {
    const headerHeight = getTimelineHeaderHeight(compact)
    rows.push({
      index: rows.length,
      kind: header.kind,
      start: currentStart,
      size: headerHeight,
      date: header.date,
      month: header.month,
      count: header.count,
      startIndex: header.startIndex,
      endIndex: header.endIndex,
      items: [],
    })
    currentStart += headerHeight + gap
  }

  const appendAsset = (item: AssetLayoutMetaItem, index: number) => {
    const aspectRatio = normalizeAspectRatio(item)
    currentItems.push({ index, id: item.id, aspectRatio })
    currentAspectSum += aspectRatio

    // 经典 justified layout：当当前行按目标高度排版后已触达容器宽度，就立即收束成一行。
    const totalGap = Math.max(0, currentItems.length - 1) * gap
    const projectedRowWidth = currentAspectSum * targetRowHeight + totalGap
    if (projectedRowWidth >= contentWidth) {
      finalizeRow(true)
    }
  }

  const timelineSections =
    timelineBuckets.length > 0 && hasCompleteTimelineBuckets(timelineBuckets, metaItems.length)
      ? buildTimelineSections(timelineBuckets, grouping, metaItems.length)
      : []

  if (timelineSections.length > 0) {
    for (const section of timelineSections) {
      for (const header of getTimelineHeaderDescriptors(section, grouping)) {
        appendHeader(header)
      }

      for (let index = section.startIndex; index < section.endIndex; index += 1) {
        const item = metaItems[index]
        if (item) {
          appendAsset(item, index)
        }
      }

      // 分组边界必须结束当前行，下一组从新行开始。
      finalizeRow(false)
    }
  } else {
    metaItems.forEach((item, index) => appendAsset(item, index))
    finalizeRow(false)
  }

  return { rows, rowIndexByAssetIndex }
}

export function useAdaptiveVirtualizer(options: UseAdaptiveVirtualizerOptions) {
  const {
    containerRef,
    containerWidth,
    targetRowHeight,
    scrollMargin,
    gap = GALLERY_CARD_GAP,
  } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  // 在 adaptive 模式里，viewSize 的语义不再是“方形卡片边长”，而是“目标行高”。
  // 外层滚动容器直接承担左右内边距，布局宽度直接使用可见内容区宽度。
  const contentWidth = computed(() => Math.max(0, containerWidth.value))
  const { layoutMetaItems, reloadLayoutMeta, ensureLayoutMetaLoaded } =
    useGalleryLayoutMeta('adaptive')
  const virtualRows = shallowRef<VirtualAdaptiveRow[]>([])
  const loadingPages = new Set<number>()

  const timelineBuckets = computed(() => {
    if (
      !store.isDateGroupingEnabled ||
      store.timelineBuckets.length === 0 ||
      !hasCompleteTimelineBuckets(store.timelineBuckets, layoutMetaItems.value.length)
    ) {
      return []
    }

    return store.timelineBuckets
  })

  const layout = computed(() =>
    buildAdaptiveRows(
      layoutMetaItems.value,
      contentWidth.value,
      targetRowHeight.value,
      gap,
      timelineBuckets.value,
      store.isCompactWindow,
      store.view.dateGrouping
    )
  )

  // 虚拟滚动的单位是“视觉行”而不是“资产”。标题行也占据自己的布局高度。
  const virtualizer = useVirtualizer<HTMLElement, HTMLElement>({
    get count() {
      return layout.value.rows.length
    },
    getScrollElement: () => containerRef.value,
    estimateSize: (index) => layout.value.rows[index]?.size ?? targetRowHeight.value,
    gap,
    get scrollMargin() {
      return scrollMargin.value
    },
    paddingStart: 0,
    paddingEnd: 16,
    overscan: 8,
  })

  function syncVirtualRows(items: ReturnType<typeof virtualizer.value.getVirtualItems>) {
    const rows = layout.value.rows
    if (items.length === 0 || rows.length === 0) {
      virtualRows.value = []
      store.setVisibleRange(undefined, undefined)
      return
    }

    // store.visibleRange 仍然以“全局 asset index”表达，供现有分页加载与选中逻辑复用。
    const visibleIndexes = items.flatMap(
      (virtualItem) => rows[virtualItem.index]?.items.map((item) => item.index) ?? []
    )

    if (visibleIndexes.length === 0) {
      store.setVisibleRange(undefined, undefined)
    } else {
      store.setVisibleRange(Math.min(...visibleIndexes), Math.max(...visibleIndexes))
    }

    virtualRows.value = items.flatMap((virtualItem): VirtualAdaptiveRow[] => {
      const row = rows[virtualItem.index]
      if (!row) {
        return []
      }

      if (row.kind !== 'assets') {
        return [
          {
            index: row.index,
            kind: row.kind,
            start: Math.round(virtualItem.start),
            size: Math.round(virtualItem.size),
            date: row.date,
            month: row.month,
            count: row.count,
            startIndex: row.startIndex,
            endIndex: row.endIndex,
            items: [],
          },
        ]
      }

      return [
        {
          index: row.index,
          kind: row.kind,
          start: Math.round(virtualItem.start),
          size: Math.round(virtualItem.size),
          items: row.items.map((item) => {
            return {
              ...item,
              asset: store.getAssetAt(item.index),
            }
          }),
        },
      ]
    })
  }

  async function loadMissingData(items: ReturnType<typeof virtualizer.value.getVirtualItems>) {
    if (items.length === 0) {
      return
    }

    // 行里每个 item 仍映射回原始结果集索引，因此分页策略可以完全复用 galleryData.loadPage。
    const rows = layout.value.rows
    const neededPages = new Set<number>()

    items.forEach((virtualItem) => {
      const row = rows[virtualItem.index]
      if (!row || row.kind !== 'assets') {
        return
      }

      row.items.forEach((item) => {
        neededPages.add(Math.floor(item.index / store.perPage) + 1)
      })
    })

    const loadPromises: Promise<void>[] = []
    neededPages.forEach((pageNum) => {
      if (!store.isPageLoaded(pageNum) && !loadingPages.has(pageNum)) {
        loadingPages.add(pageNum)
        const loadPromise = galleryData.loadPage(pageNum).finally(() => {
          loadingPages.delete(pageNum)
        })
        loadPromises.push(loadPromise)
      }
    })

    if (loadPromises.length > 0) {
      await Promise.all(loadPromises)
    }
  }

  async function init() {
    const hasReusableCache = store.totalCount > 0 && store.paginatedAssets.size > 0
    const hasReusableTimelineCache = !store.isTimelineMode || store.timelineBuckets.length > 0

    // 已有可用分页缓存时只刷新布局元数据；首次查询由完整结果替换信号触发元数据加载，
    // 避免同一次 refreshCurrentQuery 产生重复请求。
    if (hasReusableCache && hasReusableTimelineCache) {
      await ensureLayoutMetaLoaded()
      return
    }

    if (store.isTimelineMode) {
      await galleryData.loadTimelineData()
      return
    }

    await galleryData.loadAllAssets()
  }

  watch(
    () => store.queryResultVersion,
    async () => {
      await reloadLayoutMeta()
    },
    { flush: 'post' }
  )

  watch(
    () => ({
      items: virtualizer.value.getVirtualItems(),
      rows: layout.value.rows,
      paginatedAssetsVersion: store.paginatedAssetsVersion,
    }),
    async ({ items }) => {
      syncVirtualRows(items)
      await loadMissingData(items)
      syncVirtualRows(virtualizer.value.getVirtualItems())
    }
  )

  watch([layout, targetRowHeight], () => {
    // 行分布或目标高度变化后通知 virtualizer 重算总高度和可见窗口。
    virtualizer.value.measure()
  })

  function scrollToIndex(index: number, align: 'auto' | 'start' = 'auto') {
    // 灯箱返回/背景预对齐仍以 asset index 为中心语义，因此这里需要先映射到行再滚动。
    const rowIndex = layout.value.rowIndexByAssetIndex.get(index)
    if (rowIndex === undefined) {
      return
    }

    virtualizer.value.scrollToIndex(rowIndex, { align })
  }

  return {
    virtualizer,
    virtualRows,
    rows: computed(() => layout.value.rows),
    rowIndexByAssetIndex: computed(() => layout.value.rowIndexByAssetIndex),
    gap,
    init,
    scrollToIndex,
  }
}
