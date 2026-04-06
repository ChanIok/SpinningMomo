<script setup lang="ts">
import { computed } from 'vue'
import type { TimelineBucket } from '../types'
import GalleryScrollbarRail, {
  type GalleryScrollbarLabel,
  type GalleryScrollbarMarker,
} from './GalleryScrollbarRail.vue'

// Grid 时间线模式的适配层：
// 复用通用滚动轨道，只负责把“月份 bucket”翻译成轨道标记与年份标签。
const props = defineProps<{
  buckets: TimelineBucket[]
  containerHeight: number
  scrollTop: number
  viewportHeight: number
  estimatedRowHeight: number
  columns: number
  virtualizer: {
    getTotalSize: () => number
    scrollToOffset: (offset: number, options?: { behavior?: 'auto' | 'smooth' }) => void
  }
}>()

function formatMonthFull(monthStr: string): string {
  const [year, month] = monthStr.split('-')
  return `${year}年${month}月`
}

const markers = computed((): GalleryScrollbarMarker[] => {
  // Grid 时间线布局是固定行高，因此月份起点可以直接通过 index -> row -> offset 推导。
  const totalContentHeight = props.virtualizer.getTotalSize()
  if (props.buckets.length === 0 || totalContentHeight === 0) {
    return []
  }

  const result: GalleryScrollbarMarker[] = []
  let globalAssetIndex = 0

  for (const bucket of props.buckets) {
    const monthStartIndex = globalAssetIndex
    const rowIndex = Math.floor(monthStartIndex / props.columns)
    const contentOffset = rowIndex * props.estimatedRowHeight

    result.push({
      id: bucket.month,
      contentOffset,
      label: formatMonthFull(bucket.month),
    })

    globalAssetIndex += bucket.count
  }

  return result
})

const labels = computed((): GalleryScrollbarLabel[] => {
  // 年份标签只在每个年份出现的第一个月份位置渲染一次。
  const result: GalleryScrollbarLabel[] = []
  let currentYear: string | null = null

  for (const marker of markers.value) {
    const year = marker.id.split('-')[0]
    if (year && year !== currentYear) {
      result.push({
        id: year,
        text: year,
        contentOffset: marker.contentOffset,
      })
      currentYear = year
    }
  }

  return result
})
</script>

<template>
  <GalleryScrollbarRail
    :container-height="containerHeight"
    :scroll-top="scrollTop"
    :viewport-height="viewportHeight"
    :virtualizer="virtualizer"
    :markers="markers"
    :labels="labels"
  />
</template>
