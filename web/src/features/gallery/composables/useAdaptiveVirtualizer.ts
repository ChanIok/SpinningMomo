import { computed, ref, watch, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import { galleryApi } from '../api'
import { toQueryAssetsFilters } from '../queryFilters'
import type { AdaptiveLayoutRow, AdaptiveLayoutRowItem, Asset, AssetLayoutMetaItem } from '../types'

// 自适应视图的行内间距。这里故意比 grid 更紧凑，保持相册式观感。
const ADAPTIVE_GAP = 6

export interface UseAdaptiveVirtualizerOptions {
  // 原生滚动容器；adaptive 不再依赖 ScrollArea，而是直接读写真实滚动元素。
  containerRef: Ref<HTMLElement | null>
  // 内容区宽度，用来把“按比例排版”转换成真实行宽与行高。
  containerWidth: Ref<number>
}

export interface VirtualAdaptiveRowItem extends AdaptiveLayoutRowItem {
  // 真实资产数据按需分页加载；未加载到时保持 null，渲染骨架占位。
  asset: Asset | null
}

export interface VirtualAdaptiveRow {
  index: number
  start: number
  size: number
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
  gap: number
): { rows: AdaptiveLayoutRow[]; rowIndexByAssetIndex: Map<number, number> } {
  // 这一层只做“几何排版”，不关心真实 Asset 是否已加载。
  // 输入是轻量布局元数据，输出是稳定的行分布和 assetIndex -> rowIndex 映射。
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

    const totalGap = Math.max(0, currentItems.length - 1) * gap
    const maxRowHeight = Math.max(1, targetRowHeight)
    const fittedRowHeight = Math.max(1, (contentWidth - totalGap) / currentAspectSum)
    const rowHeight = justify ? fittedRowHeight : Math.min(maxRowHeight, fittedRowHeight)
    const rowIndex = rows.length

    const items: AdaptiveLayoutRowItem[] = currentItems.map((item) => {
      rowIndexByAssetIndex.set(item.index, rowIndex)
      return {
        index: item.index,
        id: item.id,
        width: item.aspectRatio * rowHeight,
        height: rowHeight,
        aspectRatio: item.aspectRatio,
      }
    })

    rows.push({
      index: rowIndex,
      start: currentStart,
      size: rowHeight,
      items,
    })

    currentStart += rowHeight + gap
    currentItems = []
    currentAspectSum = 0
  }

  metaItems.forEach((item, index) => {
    const aspectRatio = normalizeAspectRatio(item)
    currentItems.push({ index, id: item.id, aspectRatio })
    currentAspectSum += aspectRatio

    // 经典 justified layout：当当前行按目标高度排版后已触达容器宽度，就立即收束成一行。
    const totalGap = Math.max(0, currentItems.length - 1) * gap
    const projectedRowWidth = currentAspectSum * targetRowHeight + totalGap
    if (projectedRowWidth >= contentWidth) {
      finalizeRow(true)
    }
  })

  finalizeRow(false)

  return { rows, rowIndexByAssetIndex }
}

export function useAdaptiveVirtualizer(options: UseAdaptiveVirtualizerOptions) {
  const { containerRef, containerWidth } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  // 在 adaptive 模式里，viewSize 的语义不再是“方形卡片边长”，而是“目标行高”。
  const targetRowHeight = computed(() => Math.max(100, store.viewConfig.size))
  // 外层滚动容器直接承担左右内边距，布局宽度直接使用可见内容区宽度。
  const contentWidth = computed(() => Math.max(0, containerWidth.value))
  const layoutMetaItems = ref<AssetLayoutMetaItem[]>([])
  const virtualRows = ref<VirtualAdaptiveRow[]>([])
  const loadingPages = ref<Set<number>>(new Set())
  const layoutRequestId = ref(0)

  const layout = computed(() =>
    buildAdaptiveRows(
      layoutMetaItems.value,
      contentWidth.value,
      targetRowHeight.value,
      ADAPTIVE_GAP
    )
  )

  // 虚拟滚动的单位是“行”而不是“资产”。这正是 adaptive 与 masonry/grid 的核心区别。
  const virtualizer = useVirtualizer<HTMLElement, HTMLElement>({
    get count() {
      return layout.value.rows.length
    },
    getScrollElement: () => containerRef.value,
    estimateSize: (index) => layout.value.rows[index]?.size ?? targetRowHeight.value,
    gap: ADAPTIVE_GAP,
    paddingStart: 0,
    paddingEnd: 16,
    overscan: 8,
  })

  async function reloadLayoutMeta() {
    // 只要筛选/排序变化，就重新拉取轻量布局元数据，保证整批行断点稳定。
    const requestId = layoutRequestId.value + 1
    layoutRequestId.value = requestId

    const filters = toQueryAssetsFilters(store.filter, store.includeSubfolders)

    try {
      const response = await galleryApi.queryAssetLayoutMeta({
        filters,
        sortBy: store.sortBy,
        sortOrder: store.sortOrder,
      })

      if (layoutRequestId.value !== requestId) {
        return
      }

      layoutMetaItems.value = response.items
    } catch (error) {
      if (layoutRequestId.value !== requestId) {
        return
      }

      layoutMetaItems.value = []
      console.error('Failed to reload adaptive layout meta:', error)
    }
  }

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

    virtualRows.value = items.map((virtualItem) => {
      const row = rows[virtualItem.index]!
      return {
        index: row.index,
        start: virtualItem.start,
        size: virtualItem.size,
        items: row.items.map((item) => {
          const [asset] = store.getAssetsInRange(item.index, item.index)
          return {
            ...item,
            asset: asset ?? null,
          }
        }),
      }
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
      if (!row) {
        return
      }

      row.items.forEach((item) => {
        neededPages.add(Math.floor(item.index / store.perPage) + 1)
      })
    })

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

  async function init() {
    const hasReusableCache = store.totalCount > 0 && store.paginatedAssets.size > 0

    // 先拿布局元数据，再按当前查询加载可见资产页；两条链路职责分离。
    // 若已有可用分页缓存，则只更新布局元数据，避免 refreshCurrentQuery 把缓存先替换成 page1。
    if (hasReusableCache) {
      await reloadLayoutMeta()
      return
    }

    await Promise.all([reloadLayoutMeta(), galleryData.refreshCurrentQuery()])
  }

  watch(
    () => [store.filter, store.includeSubfolders, store.sortBy, store.sortOrder],
    async () => {
      await reloadLayoutMeta()
    },
    { deep: true }
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
    if (layout.value.rows.length > 0) {
      virtualizer.value.measure()
    }
  })

  function scrollToIndex(index: number) {
    // 灯箱返回/背景预对齐仍以 asset index 为中心语义，因此这里需要先映射到行再滚动。
    const rowIndex = layout.value.rowIndexByAssetIndex.get(index)
    if (rowIndex === undefined) {
      return
    }

    virtualizer.value.scrollToIndex(rowIndex, { align: 'auto' })
  }

  return {
    virtualizer,
    virtualRows,
    rows: computed(() => layout.value.rows),
    rowIndexByAssetIndex: computed(() => layout.value.rowIndexByAssetIndex),
    gap: ADAPTIVE_GAP,
    init,
    scrollToIndex,
  }
}
