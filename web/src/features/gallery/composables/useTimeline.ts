import { ref, watch, isRef, computed, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { useGalleryStore } from '../store'
import type { Asset } from '../types'

/**
 * 虚拟行类型
 */
interface VirtualRow {
  index: number
  type: 'asset-row'
  month: string
  rowInMonth: number // 在月份内的行号（从0开始）
  assets: Asset[] // 该行的资产（可能为空，未加载）
  isLoaded: boolean
}

/**
 * 月份数据
 */
interface MonthData {
  month: string
  count: number
  rows: number // 该月有多少行
  startRowIndex: number // 该月起始行索引（全局）
  assets: Asset[] | null // 该月的所有资产（未加载为null）
  isLoaded: boolean
}

/**
 * 时间线 Composable
 * 管理时间线数据、虚拟滚动和按需加载
 */
export function useTimeline(options: {
  columns: Ref<number> | number
  containerRef: Ref<HTMLElement | null>
  containerWidth?: Ref<number> | number
}) {
  const store = useGalleryStore()
  const columns = isRef(options.columns) ? options.columns : ref(options.columns)
  const containerRef = options.containerRef
  const containerWidth = options.containerWidth
    ? isRef(options.containerWidth)
      ? options.containerWidth
      : ref(options.containerWidth)
    : ref(0)

  // ============= 状态 =============
  // 从 Store 读取数据
  const buckets = computed(() => store.timelineBuckets)
  const totalPhotos = computed(() => store.timelineTotalCount)

  // 本地 UI 状态
  const monthsData = ref<Map<string, MonthData>>(new Map())
  const virtualRows = ref<VirtualRow[]>([])

  // ============= 虚拟滚动配置 =============
  // 动态计算行高（基于容器宽度和列数）
  const estimatedRowHeight = computed(() => {
    // 使用响应式的 containerWidth，如果没有则回退到 DOM 属性
    const width = containerWidth.value || containerRef.value?.clientWidth || 0
    if (width === 0) return 220 // 默认值

    const gap = 16 // gap-4 对应 16px

    // 计算单个卡片的宽度：(容器宽度 - (列数-1)*gap) / 列数
    const cardWidth = Math.floor((width - (columns.value - 1) * gap) / columns.value)

    // 卡片是正方形 (aspect-ratio: 1/1)，所以高度 = 宽度
    return cardWidth + gap
  })

  const virtualizer = useVirtualizer({
    get count() {
      return virtualRows.value.length
    },
    getScrollElement: () => containerRef.value,
    estimateSize: () => estimatedRowHeight.value,
    paddingStart: 24, // 与 GridView 的 px-6 对应
    paddingEnd: 24,
    overscan: 20, // 上下各预渲染行数
  })

  // ============= 核心方法 =============

  /**
   * 初始化 - 不再请求数据，只构建虚拟行
   */
  function init() {
    console.log('📅 时间线UI初始化:', {
      months: buckets.value.length,
      totalPhotos: totalPhotos.value,
    })
    buildVirtualRows()
  }

  /**
   * 构建虚拟行数组
   */
  function buildVirtualRows() {
    const months = new Map<string, MonthData>()
    const rows: VirtualRow[] = []
    let globalRowIndex = 0

    for (const bucket of buckets.value) {
      // 计算该月需要多少行
      const rowsInMonth = Math.ceil(bucket.count / columns.value)

      // 创建月份数据
      const monthData: MonthData = {
        month: bucket.month,
        count: bucket.count,
        rows: rowsInMonth,
        startRowIndex: globalRowIndex,
        assets: null,
        isLoaded: false,
      }
      months.set(bucket.month, monthData)

      // 为该月创建虚拟行
      for (let i = 0; i < rowsInMonth; i++) {
        rows.push({
          index: globalRowIndex,
          type: 'asset-row',
          month: bucket.month,
          rowInMonth: i,
          assets: [],
          isLoaded: false,
        })
        globalRowIndex++
      }
    }

    monthsData.value = months
    virtualRows.value = rows
  }

  /**
   * 从 Store 获取指定月份的数据并更新 UI
   */
  function syncMonthFromStore(month: string) {
    const monthData = monthsData.value.get(month)
    if (!monthData || monthData.isLoaded) {
      return
    }

    // 从 Store 获取缓存的数据（使用复合缓存键）
    const folderId = store.filter.folderId ? Number(store.filter.folderId) : undefined
    const includeSubfolders = store.includeSubfolders
    const cachedAssets = store.getMonthAssets(month, folderId, includeSubfolders)
    if (cachedAssets) {
      monthData.assets = cachedAssets
      monthData.isLoaded = true
      updateVirtualRowsForMonth(month, cachedAssets)
      console.log('✅ 月份数据同步完成:', month, cachedAssets.length)
    }
  }

  /**
   * 更新虚拟行的资产数据（将月份资产分配到各行）
   */
  function updateVirtualRowsForMonth(month: string, assets: Asset[]) {
    const monthData = monthsData.value.get(month)
    if (!monthData) return

    // 按行分组资产
    for (let rowIndex = 0; rowIndex < monthData.rows; rowIndex++) {
      const start = rowIndex * columns.value
      const end = start + columns.value
      const rowAssets = assets.slice(start, end)

      // 找到对应的虚拟行
      const virtualRow = virtualRows.value[monthData.startRowIndex + rowIndex]
      if (virtualRow) {
        virtualRow.assets = rowAssets
        virtualRow.isLoaded = true
      }
    }
  }

  /**
   * 检查并加载可见月份 - 需要外部传入加载函数
   */
  async function checkAndLoadVisibleMonths(loadMonthFn: (month: string) => Promise<void>) {
    const virtualItems = virtualizer.value.getVirtualItems()
    const visibleMonths = new Set<string>()

    // 收集需要加载的月份
    for (const virtualItem of virtualItems) {
      const row = virtualRows.value[virtualItem.index]
      if (row && !row.isLoaded) {
        visibleMonths.add(row.month)
      }
    }

    // 使用外部传入的加载函数（并行）
    await Promise.all(
      Array.from(visibleMonths).map(async (month) => {
        await loadMonthFn(month)
        // 加载完成后，从 Store 同步到 UI
        syncMonthFromStore(month)
      })
    )
  }

  /**
   * 滚动到指定偏移量（用于时间线滚动条的无极调节）
   * 使用 virtualizer.scrollToOffset 而不是 DOM scrollTo
   * 这样可以确保虚拟滚动器的状态同步
   */
  function scrollToOffset(offset: number) {
    virtualizer.value.scrollToOffset(offset, {
      align: 'start',
      behavior: 'auto',
    })
  }

  // ============= 监听器 =============

  // 监听 buckets 变化，重建虚拟行
  watch(
    buckets,
    () => {
      if (buckets.value.length > 0) {
        buildVirtualRows()
      }
    },
    { immediate: true }
  )

  // 🐛 调试：监听行高变化
  watch(estimatedRowHeight, (newHeight, oldHeight) => {
    console.log('📏 行高变化:', {
      oldHeight,
      newHeight,
      containerWidth: containerWidth.value,
      columns: columns.value,
      timestamp: new Date().toLocaleTimeString(),
    })

    // 🔄 重要：通知 virtualizer 重新测量所有项目
    if (virtualRows.value.length > 0) {
      virtualizer.value.measure()
      console.log('🔄 virtualizer 已重新测量，新的总高度:', virtualizer.value.getTotalSize())
    }
  })

  // 监听列数变化，重建虚拟行
  watch(columns, () => {
    if (buckets.value.length > 0) {
      buildVirtualRows()
    }
  })

  // ============= 返回 =============

  return {
    // 状态（从 Store 读取）
    buckets,
    totalPhotos,

    // 本地 UI 状态
    monthsData,
    virtualRows,

    // 虚拟滚动
    virtualizer,
    estimatedRowHeight,

    // 方法
    init,
    syncMonthFromStore,
    checkAndLoadVisibleMonths, // 需要传入加载函数
    scrollToOffset,
  }
}
