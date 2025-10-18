import { computed, watch, ref, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import { useGalleryData } from './useGalleryData'
import type { Asset } from '../types'

/**
 * 网格虚拟列表 Composable
 * 支持分页加载 + 虚拟滚动的混合方案
 *
 * 设计原则:
 * - 只接收 UI 布局相关参数
 * - 数据操作内部化，通过 store 和 galleryData 获取
 */
export interface UseGridVirtualizerOptions {
  containerRef: Ref<HTMLElement | null> // 滚动容器引用
  columns: Ref<number> // 列数
  containerWidth: Ref<number> // 容器宽度
}

export interface VirtualRow {
  index: number
  assets: (Asset | null)[] // null 表示数据未加载（显示骨架屏）
  start: number
  size: number
}

export function useGridVirtualizer(options: UseGridVirtualizerOptions) {
  const { containerRef, columns, containerWidth } = options

  // 内部导入数据依赖
  const store = useGalleryStore()
  const galleryData = useGalleryData()

  // 从 store 获取数据状态
  const totalCount = computed(() => store.totalCount)
  const perPage = computed(() => store.perPage)

  // 计算总行数
  const totalRows = computed(() => Math.ceil(totalCount.value / columns.value))

  // 动态计算行高（基于容器宽度和列数）
  const estimatedRowHeight = computed(() => {
    const width = containerWidth.value || containerRef.value?.clientWidth || 0
    if (width === 0) return 200 // 默认回退值

    const gap = 16 // gap-4 对应 16px

    // 计算单个卡片的宽度：(容器宽度 - (列数-1)*gap) / 列数
    const cardWidth = Math.floor((width - (columns.value - 1) * gap) / columns.value)

    // 卡片是正方形 (aspect-ratio: 1/1)，所以高度 = 宽度
    return cardWidth + gap
  })

  // 创建虚拟列表
  const virtualizer = useVirtualizer({
    get count() {
      return totalRows.value
    },
    getScrollElement: () => containerRef.value,
    estimateSize: () => estimatedRowHeight.value,
    overscan: 10,
  })

  // 虚拟行数据（包含资产或占位）
  const virtualRows = computed<VirtualRow[]>(() => {
    return virtualizer.value.getVirtualItems().map((virtualRow) => {
      const startIndex = virtualRow.index * columns.value
      const endIndex = Math.min(startIndex + columns.value - 1, totalCount.value - 1)

      const assets = galleryData.getAssetsInRange(startIndex, endIndex)

      return {
        index: virtualRow.index,
        assets: assets,
        start: virtualRow.start,
        size: virtualRow.size,
      }
    })
  })

  // 加载状态跟踪（防止重复加载）
  const loadingPages = ref<Set<number>>(new Set())

  /**
   * 监听可见项变化，触发按需加载
   */
  watch(
    () => virtualizer.value.getVirtualItems(),
    async (items) => {
      if (items.length === 0) return

      // 计算所有可见索引
      const visibleIndexes: number[] = []
      items.forEach((item) => {
        const start = item.index * columns.value
        const end = Math.min(start + columns.value, totalCount.value)
        for (let i = start; i < end; i++) {
          visibleIndexes.push(i)
        }
      })

      // 计算需要的页
      const neededPages = new Set(visibleIndexes.map((idx) => Math.floor(idx / perPage.value) + 1))

      // 加载缺失的页
      const loadPromises: Promise<void>[] = []
      neededPages.forEach((pageNum) => {
        if (!galleryData.isPageLoaded(pageNum) && !loadingPages.value.has(pageNum)) {
          loadingPages.value.add(pageNum)
          const loadPromise = galleryData.loadPage(pageNum).finally(() => {
            loadingPages.value.delete(pageNum)
          })
          loadPromises.push(loadPromise)
        }
      })

      // 等待所有加载完成（可选：如果不需要等待，移除 await）
      if (loadPromises.length > 0) {
        await Promise.all(loadPromises)
      }
    },
    { deep: true }
  )

  /**
   * 初始化 - 加载第一页数据
   */
  async function init() {
    console.log('📋 普通模式初始化:', {
      totalCount: totalCount.value,
      columns: columns.value,
    })

    // 调用 galleryData.loadAllAssets 获取总数并加载第一页
    await galleryData.loadAllAssets()
  }

  /**
   * 滚动到指定偏移量
   */
  function scrollToOffset(offset: number) {
    virtualizer.value.scrollToOffset(offset, { behavior: 'smooth' })
  }

  /**
   * 滚动到指定索引
   */
  function scrollToIndex(index: number) {
    virtualizer.value.scrollToIndex(index, { behavior: 'smooth' })
  }

  // 监听行高变化，通知 virtualizer 重新测量
  watch(estimatedRowHeight, () => {
    if (virtualRows.value.length > 0) {
      virtualizer.value.measure()
    }
  })

  // 监听列数变化，通知 virtualizer 重新测量
  watch(columns, () => {
    if (virtualRows.value.length > 0) {
      virtualizer.value.measure()
    }
  })

  return {
    virtualizer,
    virtualRows,
    totalRows,
    estimatedRowHeight,
    init,
    scrollToOffset,
    scrollToIndex,
  }
}
