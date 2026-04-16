import { computed, ref, watch, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import type { Asset } from '../types'

/**
 * 列表视图虚拟化 Composable
 * 将分页加载与虚拟滚动结合：可见区域之外的行不渲染 DOM，
 * 滚动进入视口时按需加载对应分页数据。
 */

export interface UseListVirtualizerOptions {
  /** 滚动容器元素引用 */
  containerRef: Ref<HTMLElement | null>
  /** 每行高度（px），由外部根据 viewSize 动态计算后传入 */
  rowHeight: Ref<number>
  /** 虚拟滚动的顶部偏移量，用于跳过固定表头（px） */
  scrollPaddingStart?: Ref<number>
}

export interface VirtualListItem {
  /** 资产在完整列表中的全局索引 */
  index: number
  /** 对应资产数据；未加载时为 null，渲染骨架屏 */
  asset: Asset | null
  /** 该行距滚动容器顶部的偏移量（px） */
  start: number
  /** 该行的实际高度（px） */
  size: number
}

export function useListVirtualizer(options: UseListVirtualizerOptions) {
  const { containerRef, rowHeight, scrollPaddingStart } = options

  const store = useGalleryStore()
  const galleryData = useGalleryData()

  const totalCount = computed(() => store.totalCount)
  // 正在加载中的页码集合，防止同一页被并发重复请求
  const loadingPages = ref<Set<number>>(new Set())
  const virtualItems = ref<VirtualListItem[]>([])

  const virtualizer = useVirtualizer<HTMLElement, HTMLElement>({
    get count() {
      return totalCount.value
    },
    getScrollElement: () => containerRef.value,
    estimateSize: () => rowHeight.value,
    paddingStart: 0,
    paddingEnd: 16,
    // scrollPaddingStart 使 scrollToIndex 时跳过固定表头，避免表头遮挡目标行
    get scrollPaddingStart() {
      return scrollPaddingStart?.value ?? 0
    },
    overscan: 14,
  })

  /**
   * 将 virtualizer 返回的虚拟项映射为带资产数据的 VirtualListItem，
   * 并通知 store 更新当前可见索引范围（用于分页预判）。
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
    const hasReusableCache = store.totalCount > 0 && store.paginatedAssets.size > 0
    // 从其它页面切回 gallery 时，若缓存已可用则不做全量刷新，避免 loadedPages 抖动。
    if (hasReusableCache) {
      return
    }

    await galleryData.loadAllAssets()
  }

  // 监听虚拟项变化（滚动、数据更新、行高变化）：
  // 1. 先用现有数据立即渲染（未加载项显示骨架屏）
  // 2. 异步加载缺失分页
  // 3. 加载完成后再次同步，将骨架屏替换为真实内容
  watch(
    () => ({
      items: virtualizer.value.getVirtualItems(),
      totalCount: totalCount.value,
      paginatedAssetsVersion: store.paginatedAssetsVersion,
      rowHeight: rowHeight.value,
    }),
    async ({ items, totalCount: total }) => {
      syncVirtualItems(items, total)
      await loadMissingData(items)
      syncVirtualItems(virtualizer.value.getVirtualItems(), totalCount.value)
    }
  )

  // 行高变化时通知 virtualizer 重新测量，避免布局错位
  watch(rowHeight, () => {
    if (virtualItems.value.length > 0) virtualizer.value.measure()
  })

  return {
    virtualizer,
    virtualItems,
    init,
  }
}
