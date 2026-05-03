import { computed, ref, watch, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import { useGalleryLayoutMeta } from './useGalleryLayoutMeta'
import type { Asset, AssetLayoutMetaItem } from '../types'

/**
 * 瀑布流视图虚拟化 Composable
 * 基于 @tanstack/vue-virtual 的 lanes（多列）模式实现瀑布流布局，
 * 结合分页加载：仅渲染可见列的 DOM，数据按需加载。
 *
 * 与 useGridVirtualizer 的区别：
 * - Grid 按行分组，每行高度固定（正方形卡片）
 * - Masonry 每项独立入列，高度由图片原始宽高比决定，需要实测（measureElement）
 */

/** 默认列间距（px），与 CSS gap 保持一致 */
const DEFAULT_MASONRY_GAP = 16
const MASONRY_MIN_ITEM_HEIGHT = 80
// TanStack masonry lanes 偶尔会返回比真实视口大很多的连续 range。
// 渲染与分页加载只消费视口附近几屏，避免一次性拉取大量后端分页。
const MASONRY_LOAD_BUFFER_VIEWPORTS = 2

export interface UseMasonryVirtualizerOptions {
  /** 滚动容器元素引用 */
  containerRef: Ref<HTMLElement | null>
  /** 当前列数，由外部根据容器宽度和 viewSize 动态计算 */
  columns: Ref<number>
  /** 容器宽度（px），用于计算单列宽度 */
  containerWidth: Ref<number>
  /** 卡片间距（px），水平和垂直统一使用该值 */
  gap?: number
}

export interface VirtualMasonryItem {
  /** 资产在完整列表中的全局索引 */
  index: number
  /** 对应资产数据；未加载时为 null，渲染骨架屏 */
  asset: Asset | null
  /** 该项在其所在列中的顶部偏移量（px） */
  start: number
  /** 该项的实际高度（px），由 measureElement 实测后更新 */
  size: number
  /** 所在列的索引（0-based） */
  lane: number
}

/**
 * 根据资产原始尺寸和列宽计算卡片渲染高度。
 * 未知尺寸时回退为正方形（columnWidth），最小高度为 80px。
 */
function getAssetDimensions(
  asset: Asset | null,
  meta: AssetLayoutMetaItem | null
): { width: number; height: number } | null {
  if (asset?.width && asset.height && asset.width > 0 && asset.height > 0) {
    return {
      width: asset.width,
      height: asset.height,
    }
  }

  if (meta?.width && meta.height && meta.width > 0 && meta.height > 0) {
    return {
      width: meta.width,
      height: meta.height,
    }
  }

  return null
}

function getAssetHeight(
  asset: Asset | null,
  columnWidth: number,
  meta: AssetLayoutMetaItem | null = null
): number {
  if (columnWidth <= 0) return 200

  const dimensions = getAssetDimensions(asset, meta)
  if (!dimensions) {
    return columnWidth
  }

  return Math.max(
    MASONRY_MIN_ITEM_HEIGHT,
    Math.round((columnWidth * dimensions.height) / dimensions.width)
  )
}

export function useMasonryVirtualizer(options: UseMasonryVirtualizerOptions) {
  const { containerRef, columns, containerWidth, gap = DEFAULT_MASONRY_GAP } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const totalCount = computed(() => store.totalCount)
  // 正在加载中的页码集合，防止同一页被并发重复请求
  const loadingPages = ref<Set<number>>(new Set())
  const virtualItems = ref<VirtualMasonryItem[]>([])
  const { layoutMetaItems, reloadLayoutMeta } = useGalleryLayoutMeta('masonry')

  // 单列宽度 = (容器宽度 - 列间总间距) / 列数
  const columnWidth = computed(() => {
    const width = containerWidth.value || containerRef.value?.clientWidth || 0
    if (width <= 0) return store.viewConfig.size

    const totalGap = Math.max(0, columns.value - 1) * gap
    return Math.max(1, Math.floor((width - totalGap) / Math.max(columns.value, 1)))
  })

  const itemStartByIndex = computed(() => {
    // 时间线轨道需要 asset index -> content offset 的稳定映射。
    // 这里复用 Masonry 的“最短列”规则重放一遍全量轻量布局，不触发资产分页加载。
    const startMap = new Map<number, number>()
    const laneCount = Math.max(1, columns.value)
    const laneHeights = new Array<number>(laneCount).fill(0)

    for (let index = 0; index < totalCount.value; index++) {
      let lane = 0
      for (let i = 1; i < laneCount; i++) {
        const laneHeight = laneHeights[i] ?? 0
        const currentMinLaneHeight = laneHeights[lane] ?? 0
        if (laneHeight < currentMinLaneHeight) {
          lane = i
        }
      }

      const start = laneHeights[lane] ?? 0
      startMap.set(index, start)

      const [asset] = store.getAssetsInRange(index, index)
      const itemHeight = getAssetHeight(
        asset ?? null,
        columnWidth.value,
        layoutMetaItems.value[index] ?? null
      )
      laneHeights[lane] = start + itemHeight + gap
    }

    return startMap
  })

  // 预估高度：virtualizer 初次渲染时使用，后续由 measureElement 实测覆盖
  function estimateSize(index: number): number {
    const [asset] = store.getAssetsInRange(index, index)
    return getAssetHeight(asset ?? null, columnWidth.value, layoutMetaItems.value[index] ?? null)
  }

  const virtualizer = useVirtualizer<HTMLElement, HTMLElement>({
    get count() {
      return totalCount.value
    },
    getScrollElement: () => containerRef.value,
    estimateSize,
    // measureElement 实测已渲染 DOM 的真实高度，修正瀑布流列布局
    measureElement: (element) => element.getBoundingClientRect().height,
    gap,
    get lanes() {
      return columns.value
    },
    paddingStart: 0,
    paddingEnd: 16,
    overscan: 12,
  })

  /** 计算指定列的水平偏移量（translateX），用于定位绝对布局的卡片 */
  function getLaneOffset(lane: number): number {
    return lane * (columnWidth.value + gap)
  }

  function filterItemsNearViewport(items: ReturnType<typeof virtualizer.value.getVirtualItems>) {
    // raw virtual items 是 TanStack 为“不漏掉任何 lane”扩张后的渲染范围，
    // 不能直接作为后端分页加载范围，否则异常状态下会加载全量页面。
    const container = containerRef.value
    if (!container || items.length === 0) {
      return items
    }

    const viewportHeight = container.clientHeight
    if (viewportHeight <= 0) {
      return items
    }

    const buffer = viewportHeight * MASONRY_LOAD_BUFFER_VIEWPORTS
    const viewportStart = Math.max(0, container.scrollTop - buffer)
    const viewportEnd = container.scrollTop + viewportHeight + buffer

    return items.filter((item) => item.end >= viewportStart && item.start <= viewportEnd)
  }

  /**
   * 将视口附近的虚拟项映射为带资产数据的 VirtualMasonryItem，
   * 并通知 store 更新当前可见索引范围。
   */
  function syncVirtualItems(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>,
    total: number
  ) {
    if (items.length === 0) {
      virtualItems.value = []
      store.setVisibleRange(undefined, undefined)
      return
    }

    // 使用过滤后的范围渲染 DOM，也避免 visibleRange 被 raw range 放大。
    const renderItems = filterItemsNearViewport(items)
    if (renderItems.length === 0) {
      virtualItems.value = []
      store.setVisibleRange(undefined, undefined)
      return
    }

    const indexes = renderItems.map((item) => item.index)
    store.setVisibleRange(
      Math.max(0, Math.min(...indexes)),
      Math.min(Math.max(0, total - 1), Math.max(...indexes))
    )

    virtualItems.value = renderItems.map((item) => {
      const [asset] = store.getAssetsInRange(item.index, item.index)
      return {
        index: item.index,
        asset: asset ?? null,
        start: item.start,
        size: item.size,
        lane: item.lane,
      }
    })
  }

  /**
   * 根据视口附近项，找出尚未加载的分页并并发请求。
   * 通过 loadingPages 集合避免同一页被重复触发。
   */
  async function loadMissingData(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>
  ): Promise<void> {
    if (items.length === 0) return

    const loadItems = filterItemsNearViewport(items)
    if (loadItems.length === 0) {
      return
    }

    const neededPages = new Set(loadItems.map((item) => Math.floor(item.index / store.perPage) + 1))
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

  /** 初始化：按当前排序语义加载对应数据源（时间线/普通） */
  async function init() {
    const hasReusableCache = store.totalCount > 0 && store.paginatedAssets.size > 0
    const hasReusableTimelineCache = store.timelineBuckets.length > 0 && hasReusableCache

    if (store.isTimelineMode ? hasReusableTimelineCache : hasReusableCache) {
      await reloadLayoutMeta()
      return
    }

    if (store.isTimelineMode) {
      await Promise.all([reloadLayoutMeta(), galleryData.loadTimelineData()])
      return
    }

    await Promise.all([reloadLayoutMeta(), galleryData.loadAllAssets()])
  }

  /**
   * 供模板 :ref 回调使用，将真实 DOM 元素交给 virtualizer 实测高度。
   * 瀑布流布局依赖实测高度来精确定位各列，不可省略。
   */
  function measureElement(element: Element | null) {
    virtualizer.value.measureElement(element as HTMLElement | null)
  }

  // 监听虚拟项变化（滚动、数据更新、列数/列宽变化）：
  // 1. 先用现有数据立即渲染（未加载项显示骨架屏）
  // 2. 异步加载缺失分页
  // 3. 加载完成后再次同步，将骨架屏替换为真实内容
  watch(
    () => ({
      items: virtualizer.value.getVirtualItems(),
      totalCount: totalCount.value,
      paginatedAssetsVersion: store.paginatedAssetsVersion,
      columns: columns.value,
      width: columnWidth.value,
      layoutMetaItems: layoutMetaItems.value,
    }),
    async ({ items, totalCount: total }) => {
      syncVirtualItems(items, total)
      await loadMissingData(items)
      syncVirtualItems(virtualizer.value.getVirtualItems(), totalCount.value)
    }
  )

  // 列数或列宽变化时重新测量，避免布局错位
  watch([columns, columnWidth], () => {
    if (virtualItems.value.length > 0) virtualizer.value.measure()
  })

  watch(layoutMetaItems, () => {
    if (virtualItems.value.length > 0) virtualizer.value.measure()
  })

  watch(
    () => [store.filter, store.includeSubfolders, store.sortBy, store.sortOrder],
    async () => {
      await reloadLayoutMeta()
    },
    { deep: true }
  )

  return {
    virtualizer,
    virtualItems,
    columnWidth,
    gap,
    minItemHeight: MASONRY_MIN_ITEM_HEIGHT,
    init,
    measureElement,
    getLaneOffset,
    getAssetHeight: (asset: Asset | null, index?: number) =>
      getAssetHeight(
        asset,
        columnWidth.value,
        index === undefined ? null : (layoutMetaItems.value[index] ?? null)
      ),
    itemStartByIndex,
  }
}
