import { computed, ref, watch, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import type { Asset } from '../types'

/**
 * 瀑布流视图虚拟化 Composable
 * 基于 @tanstack/vue-virtual 的 lanes（多列）模式实现瀑布流布局，
 * 结合分页加载：仅渲染可见列的 DOM，数据按需加载。
 *
 * 与 useGridVirtualizer 的区别：
 * - Grid 按行分组，每行高度固定（正方形卡片）
 * - Masonry 每项独立入列，高度由图片原始宽高比决定，需要实测（measureElement）
 */

/** 列间距（px），与 CSS gap 保持一致 */
const MASONRY_GAP = 16

export interface UseMasonryVirtualizerOptions {
  /** 滚动容器元素引用 */
  containerRef: Ref<HTMLElement | null>
  /** 当前列数，由外部根据容器宽度和 viewSize 动态计算 */
  columns: Ref<number>
  /** 容器宽度（px），用于计算单列宽度 */
  containerWidth: Ref<number>
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
function getAssetHeight(asset: Asset | null, columnWidth: number): number {
  if (columnWidth <= 0) return 200

  if (!asset || !asset.width || !asset.height || asset.width <= 0 || asset.height <= 0) {
    return columnWidth
  }

  return Math.max(80, Math.round((columnWidth * asset.height) / asset.width))
}

export function useMasonryVirtualizer(options: UseMasonryVirtualizerOptions) {
  const { containerRef, columns, containerWidth } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const totalCount = computed(() => store.totalCount)
  // 正在加载中的页码集合，防止同一页被并发重复请求
  const loadingPages = ref<Set<number>>(new Set())
  const virtualItems = ref<VirtualMasonryItem[]>([])

  // 单列宽度 = (容器宽度 - 列间总间距) / 列数
  const columnWidth = computed(() => {
    const width = containerWidth.value || containerRef.value?.clientWidth || 0
    if (width <= 0) return store.viewConfig.size

    const totalGap = Math.max(0, columns.value - 1) * MASONRY_GAP
    return Math.max(1, Math.floor((width - totalGap) / Math.max(columns.value, 1)))
  })

  // 预估高度：virtualizer 初次渲染时使用，后续由 measureElement 实测覆盖
  function estimateSize(index: number): number {
    const [asset] = store.getAssetsInRange(index, index)
    return getAssetHeight(asset ?? null, columnWidth.value)
  }

  const virtualizer = useVirtualizer<HTMLElement, HTMLElement>({
    get count() {
      return totalCount.value
    },
    getScrollElement: () => containerRef.value,
    estimateSize,
    // measureElement 实测已渲染 DOM 的真实高度，修正瀑布流列布局
    measureElement: (element) => element.getBoundingClientRect().height,
    gap: MASONRY_GAP,
    get lanes() {
      return columns.value
    },
    paddingStart: 0,
    paddingEnd: 16,
    overscan: 12,
  })

  /** 计算指定列的水平偏移量（translateX），用于定位绝对布局的卡片 */
  function getLaneOffset(lane: number): number {
    return lane * (columnWidth.value + MASONRY_GAP)
  }

  /**
   * 将 virtualizer 返回的虚拟项映射为带资产数据的 VirtualMasonryItem，
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

    const indexes = items.map((item) => item.index)
    store.setVisibleRange(
      Math.max(0, Math.min(...indexes)),
      Math.min(Math.max(0, total - 1), Math.max(...indexes))
    )

    virtualItems.value = items.map((item) => {
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
   * 根据当前可见项，找出尚未加载的分页并并发请求。
   * 通过 loadingPages 集合避免同一页被重复触发。
   */
  async function loadMissingData(
    items: ReturnType<typeof virtualizer.value.getVirtualItems>
  ): Promise<void> {
    if (items.length === 0) return

    const neededPages = new Set(items.map((item) => Math.floor(item.index / store.perPage) + 1))
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

  /** 初始化：加载总数及第一页数据 */
  async function init() {
    await galleryData.loadAllAssets()
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

  return {
    virtualizer,
    virtualItems,
    columnWidth,
    gap: MASONRY_GAP,
    init,
    measureElement,
    getLaneOffset,
    getAssetHeight: (asset: Asset | null) => getAssetHeight(asset, columnWidth.value),
  }
}
