import { isRef, computed, type Ref } from 'vue'
import type { TimelineBucket } from '../types'

/**
 * 时间线辅助工具 Composable
 * 仅提供 TimelineScrollbar 所需的月份标记计算
 * 虚拟滚动和数据加载已统一到 useGridVirtualizer
 */
export function useTimeline(options: {
  buckets: Ref<TimelineBucket[]>
  columns: Ref<number> | number
  estimatedRowHeight: Ref<number> | number
}) {
  const columns = isRef(options.columns) ? options.columns : computed(() => options.columns)
  const estimatedRowHeight = isRef(options.estimatedRowHeight)
    ? options.estimatedRowHeight
    : computed(() => options.estimatedRowHeight)
  const buckets = options.buckets

  // ============= 月份标记计算 =============

  /**
   * 计算月份标记位置（基于全局资产索引）
   * 返回每个月第一张照片在时间线上的位置
   */
  function calculateMonthMarkers() {
    const markers: Array<{ month: string; assetIndex: number; rowIndex: number }> = []
    let globalAssetIndex = 0

    for (const bucket of buckets.value) {
      // 该月第一个资产的全局索引
      const monthStartIndex = globalAssetIndex

      // 计算该资产在第几行（行号从 0 开始）
      const rowIndex = Math.floor(monthStartIndex / (Number(columns.value) || 1))

      markers.push({
        month: bucket.month,
        assetIndex: monthStartIndex,
        rowIndex: rowIndex,
      })

      globalAssetIndex += bucket.count
    }

    return markers
  }

  // ============= 返回 =============

  return {
    // 计算方法
    calculateMonthMarkers,
    
    // 用于其他计算的响应式值
    buckets,
    columns,
    estimatedRowHeight,
  }
}
