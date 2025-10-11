import { ref, watch, isRef, computed, type Ref } from 'vue'
import { useVirtualizer } from '@tanstack/vue-virtual'
import { galleryApi } from '../api'
import type { Asset, TimelineBucket } from '../types'

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
  folderId?: Ref<number | undefined> | number
  includeSubfolders?: Ref<boolean | undefined> | boolean
  columns: Ref<number> | number
  containerRef: Ref<HTMLElement | null>
  containerWidth?: Ref<number> | number // 添加响应式容器宽度
}) {
  const folderId = ref(isRef(options.folderId) ? options.folderId.value : options.folderId)
  const includeSubfolders = ref(
    isRef(options.includeSubfolders) ? options.includeSubfolders.value : options.includeSubfolders
  )
  const columns = isRef(options.columns) ? options.columns : ref(options.columns)
  const containerRef = options.containerRef
  const containerWidth = options.containerWidth
    ? isRef(options.containerWidth)
      ? options.containerWidth
      : ref(options.containerWidth)
    : ref(0)

  // ============= 状态 =============
  const buckets = ref<TimelineBucket[]>([])
  const monthsData = ref<Map<string, MonthData>>(new Map())
  const virtualRows = ref<VirtualRow[]>([])
  const isLoading = ref(false)
  const totalPhotos = ref(0)

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
    paddingStart: 24,  // 与 GridView 的 px-6 对应
    paddingEnd: 24,
    overscan: 20, // 上下各预渲染行数
  })

  // ============= 核心方法 =============

  /**
   * 初始化：获取所有月份的元数据
   */
  async function init() {
    isLoading.value = true
    try {
      const response = await galleryApi.getTimelineBuckets({
        folderId: folderId.value,
        includeSubfolders: includeSubfolders.value,
      })

      buckets.value = response.buckets
      totalPhotos.value = response.totalCount

      // 构建月份数据和虚拟行
      buildVirtualRows()

      console.log('📅 时间线初始化完成:', {
        months: buckets.value.length,
        totalPhotos: totalPhotos.value,
        totalRows: virtualRows.value.length,
      })
    } catch (error) {
      console.error('时间线初始化失败:', error)
    } finally {
      isLoading.value = false
    }
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
   * 加载指定月份的数据
   */
  async function loadMonth(month: string) {
    const monthData = monthsData.value.get(month)
    if (!monthData || monthData.isLoaded) {
      return
    }

    try {
      console.log('📸 加载月份数据:', month)

      const response = await galleryApi.getAssetsByMonth({
        month: month,
        folderId: folderId.value,
        includeSubfolders: includeSubfolders.value,
        sortOrder: 'desc',
      })

      // 更新月份数据
      monthData.assets = response.assets
      monthData.isLoaded = true

      // 更新虚拟行的资产数据
      updateVirtualRowsForMonth(month, response.assets)

      console.log('✅ 月份数据加载完成:', month, response.count)
    } catch (error) {
      console.error('加载月份数据失败:', month, error)
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
   * 检查并加载可见行的数据
   */
  async function checkAndLoadVisibleMonths() {
    const virtualItems = virtualizer.value.getVirtualItems()
    const visibleMonths = new Set<string>()

    // 收集可见行所属的月份
    for (const virtualItem of virtualItems) {
      const row = virtualRows.value[virtualItem.index]
      if (row && !row.isLoaded) {
        visibleMonths.add(row.month)
      }
    }

    // 加载这些月份（并行）
    await Promise.all(Array.from(visibleMonths).map((month) => loadMonth(month)))
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
    // 当行高变化时（比如窗口大小改变），需要告诉 virtualizer 重新计算布局
    if (virtualRows.value.length > 0) {
      virtualizer.value.measure()
      console.log('🔄 virtualizer 已重新测量，新的总高度:', virtualizer.value.getTotalSize())
    }
  })

  // 监听可见项变化，按需加载
  watch(
    () => virtualizer.value.getVirtualItems(),
    () => {
      checkAndLoadVisibleMonths()
    },
    { deep: true }
  )

  // 监听列数变化，重建虚拟行
  watch(columns, () => {
    if (buckets.value.length > 0) {
      buildVirtualRows()
      // 重新加载可见月份
      checkAndLoadVisibleMonths()
    }
  })

  // 监听文件夹筛选变化，重新初始化
  watch(
    [folderId, includeSubfolders],
    () => {
      if (buckets.value.length > 0) {
        // 清空缓存，重新初始化
        monthsData.value.clear()
        init()
      }
    },
    { deep: true }
  )

  // ============= 返回 =============

  return {
    // 状态
    buckets,
    monthsData,
    virtualRows,
    isLoading,
    totalPhotos,

    // 虚拟滚动
    virtualizer,

    // 方法
    init,
    loadMonth,
    scrollToOffset,

    // 动态行高（用于 TimelineScrollbar）
    estimatedRowHeight,
  }
}
