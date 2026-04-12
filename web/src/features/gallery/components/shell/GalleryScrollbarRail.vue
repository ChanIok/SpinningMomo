<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useThrottleFn } from '@vueuse/core'

// 通用滚动轨道的数据模型。
// 轨道本身不关心“这是月份还是别的业务标记”，只关心内容坐标与展示文案。
export interface GalleryScrollbarMarker {
  id: string
  contentOffset: number
  label?: string
}

export interface GalleryScrollbarLabel {
  id: string
  text: string
  contentOffset: number
}

const props = withDefaults(
  defineProps<{
    containerHeight: number
    scrollTop: number
    viewportHeight: number
    virtualizer: {
      getTotalSize: () => number
      scrollToOffset: (offset: number, options?: { behavior?: 'auto' | 'smooth' }) => void
    }
    markers?: GalleryScrollbarMarker[]
    labels?: GalleryScrollbarLabel[]
  }>(),
  {
    markers: () => [],
    labels: () => [],
  }
)

const timelineRef = ref<HTMLElement | null>(null)

// 顶底各留一段安全区，避免年份标签和指示器紧贴边缘。
const CONTENT_OFFSET_TOP = 24
const CONTENT_OFFSET_BOTTOM = 24

const availableHeight = computed(() => {
  return Math.max(0, props.containerHeight - CONTENT_OFFSET_TOP - CONTENT_OFFSET_BOTTOM)
})

// 拖拽和 hover 都以轨道坐标表达，再统一映射回内容坐标。
const isDragging = ref(false)
const hoverY = ref<number | null>(null)

function contentToTimeline(contentY: number): number {
  const totalContentHeight = props.virtualizer.getTotalSize()
  if (totalContentHeight === 0 || availableHeight.value === 0) {
    return CONTENT_OFFSET_TOP
  }

  const ratio = contentY / totalContentHeight
  return CONTENT_OFFSET_TOP + ratio * availableHeight.value
}

function timelineToContent(timelineY: number): number {
  const totalContentHeight = props.virtualizer.getTotalSize()
  if (availableHeight.value === 0) {
    return 0
  }

  const adjustedY = Math.max(0, timelineY - CONTENT_OFFSET_TOP)
  return (adjustedY / availableHeight.value) * totalContentHeight
}

// 把业务侧传入的“内容偏移量”统一映射成轨道坐标，后续模板无需了解内容总高度。
const mappedMarkers = computed(() => {
  return props.markers.map((marker) => ({
    ...marker,
    offsetTop: contentToTimeline(marker.contentOffset),
  }))
})

const mappedLabels = computed(() => {
  return props.labels.map((label) => ({
    ...label,
    offsetTop: Math.max(CONTENT_OFFSET_TOP - 25, contentToTimeline(label.contentOffset) - 25),
  }))
})

const indicatorTop = computed(() => {
  return contentToTimeline(props.scrollTop)
})

// hover 提示复用最近邻 marker；没有 marker 时，轨道仍可作为普通滚动条使用。
const hoverLabel = computed(() => {
  if (hoverY.value === null || mappedMarkers.value.length === 0) {
    return null
  }

  let closestMarker: (typeof mappedMarkers.value)[number] | null = null
  let minDistance = Infinity

  for (const marker of mappedMarkers.value) {
    const distance = Math.abs(marker.offsetTop - hoverY.value)
    if (distance < minDistance) {
      minDistance = distance
      closestMarker = marker
    }
  }

  return closestMarker?.label ?? null
})

function mapTimelineToContent(timelineY: number): number {
  return timelineToContent(timelineY)
}

// 拖动使用节流，避免在高频 mousemove 下持续触发大范围重排。
const throttledScroll = useThrottleFn((y: number) => {
  const targetScrollTop = mapTimelineToContent(y)
  props.virtualizer.scrollToOffset(targetScrollTop, { behavior: 'auto' })
}, 16)

function handleMouseMove(event: MouseEvent) {
  if (!timelineRef.value) {
    return
  }

  const rect = timelineRef.value.getBoundingClientRect()
  hoverY.value = event.clientY - rect.top
}

function handleMouseLeave() {
  if (!isDragging.value) {
    hoverY.value = null
  }
}

function handleGlobalMouseMove(event: MouseEvent) {
  if (!isDragging.value || !timelineRef.value) {
    return
  }

  const rect = timelineRef.value.getBoundingClientRect()
  const relativeY = event.clientY - rect.top
  const clampedY = Math.max(0, Math.min(relativeY, rect.height))
  throttledScroll(clampedY)
}

function handleMouseDown(event: MouseEvent) {
  if (!timelineRef.value) {
    return
  }

  isDragging.value = true
  const rect = timelineRef.value.getBoundingClientRect()
  const relativeY = event.clientY - rect.top
  throttledScroll(relativeY)
}

function handleGlobalMouseUp() {
  isDragging.value = false
}

function handleWheel(event: WheelEvent) {
  event.preventDefault()

  // 在轨道上滚轮时，直接把增量转发给内容区，保持与主视图一致的滚动手感。
  const newScrollTop = props.scrollTop + event.deltaY
  const totalContentHeight = props.virtualizer.getTotalSize()
  const maxScrollTop = Math.max(0, totalContentHeight - props.viewportHeight)
  const clampedScrollTop = Math.max(0, Math.min(newScrollTop, maxScrollTop))
  props.virtualizer.scrollToOffset(clampedScrollTop, { behavior: 'auto' })
}

onMounted(() => {
  document.addEventListener('mousemove', handleGlobalMouseMove)
  document.addEventListener('mouseup', handleGlobalMouseUp)
})

onUnmounted(() => {
  document.removeEventListener('mousemove', handleGlobalMouseMove)
  document.removeEventListener('mouseup', handleGlobalMouseUp)
})
</script>

<template>
  <div
    ref="timelineRef"
    class="timeline-scrollbar w-10 transition-all select-none"
    @mousedown="handleMouseDown"
    @mousemove="handleMouseMove"
    @mouseleave="handleMouseLeave"
    @wheel="handleWheel"
  >
    <div class="relative h-full">
      <div
        v-for="marker in mappedMarkers"
        :key="marker.id"
        class="pointer-events-none absolute right-2 h-1.5 w-1.5 rounded-full bg-border"
        :style="{ top: `${marker.offsetTop - 3}px` }"
      />

      <div
        v-for="label in mappedLabels"
        :key="label.id"
        class="pointer-events-none absolute right-0 left-0 px-2 py-1 text-right text-xs text-foreground"
        :style="{ top: `${label.offsetTop}px` }"
      >
        {{ label.text }}
      </div>

      <div
        v-if="hoverY !== null && !isDragging"
        class="pointer-events-none absolute right-1 left-2 rounded-sm bg-primary/40"
        :style="{ top: `${hoverY - 2}px`, height: '4px' }"
      />

      <div
        class="pointer-events-none absolute right-1 left-2 rounded-sm bg-primary shadow-lg"
        :style="{ top: `${indicatorTop - 2}px`, height: '4px' }"
      />

      <div
        v-if="hoverLabel"
        class="animate-fade-in pointer-events-none absolute -left-20 z-20 rounded-sm bg-popover/90 px-2 text-xs leading-6 text-popover-foreground shadow-md"
        :style="{ top: `${hoverY! - 12}px`, height: '24px' }"
      >
        {{ hoverLabel }}
      </div>
    </div>
  </div>
</template>

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
