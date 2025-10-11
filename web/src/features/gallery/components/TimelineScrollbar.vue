<template>
  <div
    ref="timelineRef"
    class="timeline-scrollbar w-10 bg-background/80 backdrop-blur transition-all select-none"
    @mousedown="handleMouseDown"
    @mousemove="handleMouseMove"
    @mouseleave="handleMouseLeave"
    @wheel="handleWheel"
  >
    <div class="relative h-full">
      <!-- 月份圆点 -->
      <div
        v-for="marker in monthMarkers"
        :key="marker.month"
        class="pointer-events-none absolute right-2 h-1.5 w-1.5 rounded-full bg-border"
        :style="{
          top: `${marker.offsetTop - 3}px`,
        }"
      />

      <!-- 年份标签 -->
      <div
        v-for="year in years"
        :key="year.year"
        class="pointer-events-none absolute right-0 left-0 px-2 py-1 text-right text-xs text-foreground"
        :style="{
          top: `${year.offsetTop}px`,
        }"
      >
        {{ year.year }}
      </div>

      <!-- Hover 预览横杠（最低层级） -->
      <div
        v-if="hoverY !== null && !isDragging"
        class="pointer-events-none absolute right-1 left-2 rounded-sm bg-primary/40"
        :style="{
          top: `${hoverY - 2}px`,
          height: '4px',
        }"
      />

      <!-- 视口指示器（横杠，视频播放器风格） -->
      <div
        class="pointer-events-none absolute right-1 left-2 rounded-sm bg-primary shadow-lg"
        :style="{
          top: `${indicatorTop - 2}px`,
          height: '4px',
        }"
      />

      <!-- Tooltip（自定义浮层） -->
      <div
        v-if="hoverMonth"
        class="animate-fade-in pointer-events-none absolute -left-20 z-20 rounded-sm bg-popover/90 px-2 text-xs leading-6 text-popover-foreground shadow-md"
        :style="{
          top: `${hoverY! - 12}px`,
          height: '24px',
        }"
      >
        {{ hoverMonth }}
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, onMounted, onUnmounted } from 'vue'
import { useThrottleFn } from '@vueuse/core'
import type { TimelineBucket } from '../types'

// 类型定义
interface MonthMarker {
  month: string
  offsetTop: number // 在时间线上的Y位置
}

interface YearLabel {
  year: string
  offsetTop: number
}

// Props定义
const props = defineProps<{
  buckets: TimelineBucket[] // 月份元数据（包含 month 和 count）
  totalContentHeight: number // 虚拟列表总高度（virtualizer.getTotalSize()）
  containerHeight: number // 滚动条容器高度
  scrollTop: number // 当前滚动位置
  viewportHeight: number // 视口高度
  estimatedRowHeight: number // 单行预估高度
  columns: number // 列数
  onScrollToOffset: (offset: number) => void // 滚动到指定偏移量
}>()

// 模板引用
const timelineRef = ref<HTMLElement | null>(null)

// ============= 坐标系统配置 =============
const CONTENT_OFFSET_TOP = 24 // 顶部预留空间（年份标签）
const CONTENT_OFFSET_BOTTOM = 24 // 底部预留空间

// 计算可用高度（去除顶部和底部偏移）
const availableHeight = computed(() => {
  return Math.max(0, props.containerHeight - CONTENT_OFFSET_TOP - CONTENT_OFFSET_BOTTOM)
})

// ============= 交互状态 =============
const isDragging = ref(false)
const hoverY = ref<number | null>(null)
const dragStartInTimeline = ref(false) // 标记是否在时间线内开始拖动

// ============= 坐标映射函数 =============

/**
 * 内容坐标 → 时间线坐标（考虑偏移）
 */
function contentToTimeline(contentY: number): number {
  if (props.totalContentHeight === 0 || availableHeight.value === 0) {
    return CONTENT_OFFSET_TOP
  }
  const ratio = contentY / props.totalContentHeight
  return CONTENT_OFFSET_TOP + ratio * availableHeight.value
}

/**
 * 时间线坐标 → 内容坐标（考虑偏移）
 */
function timelineToContent(timelineY: number): number {
  if (availableHeight.value === 0) return 0
  const adjustedY = Math.max(0, timelineY - CONTENT_OFFSET_TOP)
  return (adjustedY / availableHeight.value) * props.totalContentHeight
}

// ============= 计算时间线刻度数据 =============

/**
 * 计算每个月在时间线上的标记位置
 * 使用简单的线性映射：内容高度 → 时间线高度
 */
const monthMarkers = computed((): MonthMarker[] => {
  if (props.buckets.length === 0 || props.totalContentHeight === 0) return []

  const markers: MonthMarker[] = []
  let currentRowIndex = 0

  for (const bucket of props.buckets) {
    // 计算该月在虚拟列表中的起始位置（内容坐标）
    const contentY = currentRowIndex * props.estimatedRowHeight

    // 使用映射函数转换到时间线坐标
    const timelineY = contentToTimeline(contentY)

    markers.push({
      month: bucket.month,
      offsetTop: timelineY,
    })

    // 累加该月的行数
    const rowsInMonth = Math.ceil(bucket.count / props.columns)
    currentRowIndex += rowsInMonth
  }

  return markers
})

/**
 * 提取年份标签
 */
const years = computed((): YearLabel[] => {
  if (monthMarkers.value.length === 0) return []

  const yearLabels: YearLabel[] = []
  let currentYear: string | null = null

  for (const marker of monthMarkers.value) {
    const year = marker.month.split('-')[0]
    if (year && year !== currentYear) {
      yearLabels.push({
        year: year,
        offsetTop: Math.max(CONTENT_OFFSET_TOP - 25, marker.offsetTop - 25), // 年份标签在第一个月上方，但不超出顶部边界
      })
      currentYear = year
    }
  }

  return yearLabels
})

/**
 * 计算视口指示器位置（内容坐标 → 时间线坐标）
 * 使用映射函数，考虑偏移量
 */
const indicatorTop = computed(() => {
  return contentToTimeline(props.scrollTop)
})

// ============= Hover 预览 =============

/**
 * 计算 hover 位置对应的月份
 */
const hoverMonth = computed(() => {
  if (hoverY.value === null) return null

  // 找到最接近的月份标记
  let closestMarker: MonthMarker | null = null
  let minDistance = Infinity

  for (const marker of monthMarkers.value) {
    const distance = Math.abs(marker.offsetTop - hoverY.value)
    if (distance < minDistance) {
      minDistance = distance
      closestMarker = marker
    }
  }

  return closestMarker ? formatMonthFull(closestMarker.month) : null
})

/**
 * 处理鼠标移动（更新 hover 状态）
 */
function handleMouseMove(event: MouseEvent) {
  if (!timelineRef.value) return

  const rect = timelineRef.value.getBoundingClientRect()
  hoverY.value = event.clientY - rect.top
}

/**
 * 处理鼠标离开（清除 hover 状态）
 */
function handleMouseLeave() {
  hoverY.value = null
}

// ============= 拖动处理 =============

/**
 * 节流的滚动处理（16ms = 60fps）
 */
const throttledScroll = useThrottleFn((y: number) => {
  const targetScrollTop = mapTimelineToContent(y)
  props.onScrollToOffset(targetScrollTop)
}, 16)

/**
 * 处理全局鼠标移动（拖动时）
 */
function handleGlobalMouseMove(event: MouseEvent) {
  if (!isDragging.value || !timelineRef.value) return

  // 计算鼠标相对于时间线容器的位置
  const rect = timelineRef.value.getBoundingClientRect()
  const relativeY = event.clientY - rect.top

  // 限制在容器范围内（clamp）
  const clampedY = Math.max(0, Math.min(relativeY, rect.height))

  // 执行节流滚动
  throttledScroll(clampedY)
}

/**
 * 处理鼠标按下（在时间线内）
 */
function handleMouseDown(event: MouseEvent) {
  if (!timelineRef.value) return

  isDragging.value = true
  dragStartInTimeline.value = true

  // 立即执行一次滚动（点击效果）
  const rect = timelineRef.value.getBoundingClientRect()
  const relativeY = event.clientY - rect.top
  throttledScroll(relativeY)
}

/**
 * 处理鼠标释放（全局）
 */
function handleGlobalMouseUp() {
  isDragging.value = false
  dragStartInTimeline.value = false
}

// 生命周期：注册/注销全局事件监听器
onMounted(() => {
  document.addEventListener('mousemove', handleGlobalMouseMove)
  document.addEventListener('mouseup', handleGlobalMouseUp)
})

onUnmounted(() => {
  document.removeEventListener('mousemove', handleGlobalMouseMove)
  document.removeEventListener('mouseup', handleGlobalMouseUp)
})

// ============= 滚轮处理 =============

/**
 * 处理滚轮事件（在时间线上滚动时同步到内容区）
 */
function handleWheel(event: WheelEvent) {
  event.preventDefault()

  // 计算新的滚动位置（当前位置 + 滚轮增量）
  const newScrollTop = props.scrollTop + event.deltaY

  // 计算最大滚动距离和边界检查
  const maxScrollTop = Math.max(0, props.totalContentHeight - props.viewportHeight)
  const clampedScrollTop = Math.max(0, Math.min(newScrollTop, maxScrollTop))

  // 通过回调触发滚动
  props.onScrollToOffset(clampedScrollTop)
}

// ============= 点击处理 =============

/**
 * 映射时间线位置到内容位置（用于滚动）
 * 使用映射函数，考虑偏移量
 */
function mapTimelineToContent(timelineY: number): number {
  return timelineToContent(timelineY)
}

/**
 * 格式化月份显示（完整格式，用于 Tooltip）
 */
function formatMonthFull(monthStr: string): string {
  const [year, month] = monthStr.split('-')
  return `${year}年${month}月`
}
</script>

<style scoped>
@keyframes fade-in {
  from {
    opacity: 0;
  }
  to {
    opacity: 1;
  }
}

.animate-fade-in {
  animation: fade-in 0.15s ease-in-out;
}
</style>
