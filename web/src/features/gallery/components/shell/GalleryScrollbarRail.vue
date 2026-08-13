<script setup lang="ts">
import { computed, onUnmounted, ref, watch } from 'vue'
import { useThrottleFn } from '@vueuse/core'
import { ChevronDown, ChevronUp } from '@lucide/vue'

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
    scrollContainer?: HTMLElement | null
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
const MOBILE_HANDLE_HEIGHT = 56
const TIMELINE_LABEL_HEIGHT = 24
const TIMELINE_LABEL_GAP = 4
const MOBILE_COLLAPSE_DELAY = 3000

const availableHeight = computed(() => {
  return Math.max(0, props.containerHeight - CONTENT_OFFSET_TOP - CONTENT_OFFSET_BOTTOM)
})

const effectiveViewportHeight = computed(() => props.viewportHeight || props.containerHeight)

const fallbackScrollRange = computed(() =>
  Math.max(0, props.virtualizer.getTotalSize() - effectiveViewportHeight.value)
)

const scrollRange = ref(0)
let scrollResizeObserver: ResizeObserver | null = null

function syncScrollRange() {
  const container = props.scrollContainer
  const actualRange = container ? Math.max(0, container.scrollHeight - container.clientHeight) : 0

  scrollRange.value = actualRange > 0 ? actualRange : fallbackScrollRange.value
}

watch(
  () => [props.scrollContainer, props.scrollTop, props.containerHeight, props.viewportHeight],
  syncScrollRange,
  { immediate: true }
)

watch(
  () => props.scrollContainer,
  (container) => {
    scrollResizeObserver?.disconnect()
    scrollResizeObserver = null

    if (container && typeof ResizeObserver !== 'undefined') {
      scrollResizeObserver = new ResizeObserver(syncScrollRange)
      scrollResizeObserver.observe(container)
      if (container.firstElementChild) {
        scrollResizeObserver.observe(container.firstElementChild)
      }
    }

    syncScrollRange()
  },
  { immediate: true }
)

const hasScrollableContent = computed(() => scrollRange.value > 0)

// 拖拽和 hover 都以轨道坐标表达，再统一映射回内容坐标。
const isDragging = ref(false)
const isHandleVisible = ref(false)
const isTrackVisible = ref(false)
const activePointerId = ref<number | null>(null)
const hoverY = ref<number | null>(null)
let trackCollapseTimer: number | null = null
let handleCollapseTimer: number | null = null

watch(
  () => props.scrollTop,
  (newVal, oldVal) => {
    if (newVal === oldVal || !hasScrollableContent.value) {
      return
    }

    // 模式一：划动内容区，只显现滑块，绝不出轨道
    if (!isDragging.value) {
      isTrackVisible.value = false
      clearTrackCollapseTimer()

      isHandleVisible.value = true
      scheduleHandleCollapse(3000)
    }
  }
)

function contentToTimeline(contentY: number): number {
  if (scrollRange.value === 0 || availableHeight.value === 0) {
    return CONTENT_OFFSET_TOP
  }

  const ratio = Math.max(0, Math.min(contentY, scrollRange.value)) / scrollRange.value
  return CONTENT_OFFSET_TOP + ratio * availableHeight.value
}

const mobileRailTop = computed(() => CONTENT_OFFSET_TOP)
const mobileRailBottom = computed(() =>
  Math.max(mobileRailTop.value, props.containerHeight - CONTENT_OFFSET_BOTTOM)
)
const mobileThumbTravel = computed(() =>
  Math.max(0, mobileRailBottom.value - mobileRailTop.value - MOBILE_HANDLE_HEIGHT)
)

function getScrollProgress(contentOffset: number): number {
  if (scrollRange.value === 0) {
    return 0
  }

  return Math.max(0, Math.min(contentOffset, scrollRange.value)) / scrollRange.value
}

function mobileThumbTopForOffset(contentOffset: number): number {
  return mobileRailTop.value + getScrollProgress(contentOffset) * mobileThumbTravel.value
}

function mobilePositionForOffset(contentOffset: number): number {
  return mobileThumbTopForOffset(contentOffset) + MOBILE_HANDLE_HEIGHT / 2
}

function mobileTimelineToContent(timelineY: number): number {
  if (scrollRange.value === 0 || mobileThumbTravel.value === 0) {
    return 0
  }

  const thumbCenter = Math.max(
    mobileRailTop.value + MOBILE_HANDLE_HEIGHT / 2,
    Math.min(mobileRailBottom.value - MOBILE_HANDLE_HEIGHT / 2, timelineY)
  )
  const progress =
    (thumbCenter - mobileRailTop.value - MOBILE_HANDLE_HEIGHT / 2) / mobileThumbTravel.value

  return progress * scrollRange.value
}

function isMobileViewport(): boolean {
  return typeof window !== 'undefined' && window.matchMedia('(max-width: 639px)').matches
}

function timelineToContent(timelineY: number): number {
  if (scrollRange.value === 0 || availableHeight.value === 0) {
    return 0
  }

  if (isMobileViewport()) {
    return mobileTimelineToContent(timelineY)
  }

  const adjustedY = Math.max(0, Math.min(availableHeight.value, timelineY - CONTENT_OFFSET_TOP))
  return (adjustedY / availableHeight.value) * scrollRange.value
}

// 把业务侧传入的“内容偏移量”统一映射成轨道坐标，后续模板无需了解内容总高度。
const mappedMarkers = computed(() => {
  return props.markers.map((marker) => ({
    ...marker,
    offsetTop: contentToTimeline(marker.contentOffset),
  }))
})

const mappedMobileMarkers = computed(() => {
  return props.markers.map((marker) => ({
    ...marker,
    offsetTop: mobilePositionForOffset(marker.contentOffset),
  }))
})

type MappedScrollbarLabel = GalleryScrollbarLabel & { offsetTop: number }

// 桌面和移动端共用静态碰撞规则：年份越新，优先保留。
function filterOverlappingLabels(labels: MappedScrollbarLabel[]) {
  const visibleLabels: MappedScrollbarLabel[] = []

  for (const label of [...labels].sort((a, b) => Number(b.id) - Number(a.id))) {
    const overlapsVisibleLabel = visibleLabels.some(
      (visibleLabel) =>
        Math.abs(visibleLabel.offsetTop - label.offsetTop) <
        TIMELINE_LABEL_HEIGHT + TIMELINE_LABEL_GAP
    )

    if (!overlapsVisibleLabel) {
      visibleLabels.push(label)
    }
  }

  return visibleLabels.sort((a, b) => a.offsetTop - b.offsetTop)
}

const mappedLabels = computed(() => {
  return filterOverlappingLabels(
    props.labels.map((label) => ({
      ...label,
      offsetTop: Math.max(CONTENT_OFFSET_TOP - 25, contentToTimeline(label.contentOffset) - 25),
    }))
  )
})

const mappedMobileLabels = computed(() => {
  const maxTop = Math.max(0, props.containerHeight - TIMELINE_LABEL_HEIGHT)

  return filterOverlappingLabels(
    props.labels.map((label) => ({
      ...label,
      offsetTop: Math.max(
        0,
        Math.min(mobilePositionForOffset(label.contentOffset) - TIMELINE_LABEL_HEIGHT / 2, maxTop)
      ),
    }))
  )
})

function findClosestMarker(
  markers: Array<{ label?: string; offsetTop: number }>,
  position: number
) {
  let closestMarker: (typeof markers)[number] | null = null
  let minDistance = Infinity

  for (const marker of markers) {
    const distance = Math.abs(marker.offsetTop - position)
    if (distance < minDistance) {
      minDistance = distance
      closestMarker = marker
    }
  }

  return closestMarker
}

const indicatorTop = computed(() => {
  return contentToTimeline(props.scrollTop)
})

const mobileIndicatorTop = computed(() => {
  return mobilePositionForOffset(props.scrollTop)
})

const mobileHandleTop = computed(() => {
  return mobileThumbTopForOffset(props.scrollTop)
})

// hover 提示复用最近邻 marker；没有 marker 时，轨道仍可作为普通滚动条使用。
const hoverLabel = computed(() => {
  if (hoverY.value === null || mappedMarkers.value.length === 0) {
    return null
  }

  return findClosestMarker(mappedMarkers.value, hoverY.value)?.label ?? null
})

const activeLabel = computed(() => {
  if (mappedMobileMarkers.value.length === 0) {
    return null
  }

  return findClosestMarker(mappedMobileMarkers.value, mobileIndicatorTop.value)?.label ?? null
})

const activeLabelTop = computed(() => {
  const maxTop = Math.max(0, props.containerHeight - TIMELINE_LABEL_HEIGHT)
  return Math.max(0, Math.min(mobileIndicatorTop.value - TIMELINE_LABEL_HEIGHT / 2, maxTop))
})

// 拖动使用节流，避免在高频 pointermove 下持续触发大范围重排。
const throttledScroll = useThrottleFn((y: number) => {
  const targetScrollTop = timelineToContent(y)
  props.virtualizer.scrollToOffset(targetScrollTop, { behavior: 'auto' })
}, 16)

function clearTrackCollapseTimer() {
  if (trackCollapseTimer !== null) {
    window.clearTimeout(trackCollapseTimer)
    trackCollapseTimer = null
  }
}

function clearHandleCollapseTimer() {
  if (handleCollapseTimer !== null) {
    window.clearTimeout(handleCollapseTimer)
    handleCollapseTimer = null
  }
}

function clearAllCollapseTimers() {
  clearTrackCollapseTimer()
  clearHandleCollapseTimer()
}

function scheduleHandleCollapse(delay = 3000) {
  clearHandleCollapseTimer()
  handleCollapseTimer = window.setTimeout(() => {
    handleCollapseTimer = null
    isHandleVisible.value = false
  }, delay)
}

function scheduleTwoStageCollapse() {
  clearAllCollapseTimers()

  // 阶段 1：松手 3 秒后关闭背景轨道（退回到模式一形态）
  trackCollapseTimer = window.setTimeout(() => {
    trackCollapseTimer = null
    isTrackVisible.value = false
  }, 3000)

  // 阶段 2：松手 6 秒后（即轨道隐去后再过 3 秒）关闭滑块
  handleCollapseTimer = window.setTimeout(() => {
    handleCollapseTimer = null
    isHandleVisible.value = false
  }, 6000)
}

function updateHoverPosition(clientY: number) {
  if (!timelineRef.value) {
    return
  }

  const rect = timelineRef.value.getBoundingClientRect()
  hoverY.value = Math.max(0, Math.min(clientY - rect.top, rect.height))
}

function handleMouseMove(event: MouseEvent) {
  updateHoverPosition(event.clientY)
}

function handleMouseLeave() {
  if (!isDragging.value) {
    hoverY.value = null
  }
}

function handlePointerDown(event: PointerEvent) {
  if (
    !timelineRef.value ||
    !hasScrollableContent.value ||
    activePointerId.value !== null ||
    (event.pointerType === 'mouse' && event.button !== 0)
  ) {
    return
  }

  clearAllCollapseTimers()
  isHandleVisible.value = true
  isTrackVisible.value = true
  activePointerId.value = event.pointerId
  isDragging.value = true
  timelineRef.value.setPointerCapture(event.pointerId)

  const rect = timelineRef.value.getBoundingClientRect()
  const relativeY = event.clientY - rect.top
  updateHoverPosition(event.clientY)
  throttledScroll(relativeY)
  event.preventDefault()
}

function handlePointerMove(event: PointerEvent) {
  if (!isDragging.value || activePointerId.value !== event.pointerId || !timelineRef.value) {
    return
  }

  const rect = timelineRef.value.getBoundingClientRect()
  const relativeY = event.clientY - rect.top
  const clampedY = Math.max(0, Math.min(relativeY, rect.height))
  updateHoverPosition(event.clientY)
  throttledScroll(clampedY)
  event.preventDefault()
}

function finishPointerInteraction(event: PointerEvent) {
  if (activePointerId.value !== event.pointerId) {
    return
  }

  activePointerId.value = null
  isDragging.value = false

  if (timelineRef.value?.hasPointerCapture(event.pointerId)) {
    timelineRef.value.releasePointerCapture(event.pointerId)
  }

  scheduleTwoStageCollapse()
}

function handleLostPointerCapture(event: PointerEvent) {
  finishPointerInteraction(event)
}

function handleWheel(event: WheelEvent) {
  event.preventDefault()

  // 在轨道上滚轮时，直接把增量转发给内容区，保持与主视图一致的滚动手感。
  const newScrollTop = props.scrollTop + event.deltaY
  const clampedScrollTop = Math.max(0, Math.min(newScrollTop, scrollRange.value))
  props.virtualizer.scrollToOffset(clampedScrollTop, { behavior: 'auto' })
}

onUnmounted(() => {
  clearAllCollapseTimers()
  scrollResizeObserver?.disconnect()
})
</script>

<template>
  <div
    ref="timelineRef"
    :class="[
      'timeline-scrollbar h-full w-10 select-none',
      {
        'timeline-scrollbar-track-active': isTrackVisible,
        'timeline-scrollbar-handle-active': isHandleVisible,
        'timeline-scrollbar-disabled': !hasScrollableContent,
      },
    ]"
    @pointerdown="handlePointerDown"
    @pointermove="handlePointerMove"
    @pointerup="finishPointerInteraction"
    @pointercancel="finishPointerInteraction"
    @lostpointercapture="handleLostPointerCapture"
    @mousemove="handleMouseMove"
    @mouseleave="handleMouseLeave"
    @wheel="handleWheel"
  >
    <div class="relative h-full">
      <div class="timeline-rail-layer">
        <div
          v-if="hasScrollableContent"
          class="timeline-mobile-track pointer-events-none absolute top-6 bottom-6 rounded-full"
        />

        <div
          v-for="marker in mappedMarkers"
          :key="marker.id"
          class="timeline-marker timeline-desktop-marker pointer-events-none absolute right-2 h-1.5 w-1.5 rounded-full bg-border"
          :style="{ top: `${marker.offsetTop - 3}px` }"
        />

        <div
          v-for="marker in mappedMobileMarkers"
          :key="`mobile-${marker.id}`"
          class="timeline-marker timeline-mobile-marker pointer-events-none absolute right-2 h-1.5 w-1.5 rounded-full bg-border"
          :style="{ top: `${marker.offsetTop - 3}px` }"
        />

        <div
          v-if="hasScrollableContent"
          class="timeline-mobile-handle pointer-events-none absolute rounded-full text-[var(--timeline-surface-foreground)]"
          :style="{ top: `${mobileHandleTop}px`, height: `${MOBILE_HANDLE_HEIGHT}px` }"
        >
          <ChevronUp class="h-2.5 w-2.5 shrink-0 opacity-80" />
          <ChevronDown class="h-2.5 w-2.5 shrink-0 opacity-80" />
        </div>
      </div>

      <div
        v-for="label in mappedLabels"
        :key="label.id"
        class="timeline-label timeline-desktop-label pointer-events-none absolute right-0 left-0 px-2 py-1 text-right text-xs text-foreground"
        :style="{ top: `${label.offsetTop}px` }"
      >
        {{ label.text }}
      </div>

      <div
        v-for="label in mappedMobileLabels"
        :key="`mobile-${label.id}`"
        class="timeline-label timeline-mobile-label pointer-events-none absolute right-0 left-0 px-2 py-1 text-right text-xs text-foreground"
        :style="{ top: `${label.offsetTop}px` }"
      >
        {{ label.text }}
      </div>

      <div
        v-if="hoverY !== null && !isDragging"
        class="timeline-desktop-hover-indicator timeline-hover-indicator pointer-events-none absolute right-1 left-2 rounded-sm bg-primary/40"
        :style="{ top: `${hoverY - 2}px`, height: '4px' }"
      />

      <div
        class="timeline-desktop-position-line pointer-events-none absolute right-1 left-2 rounded-sm bg-primary shadow-lg"
        :style="{ top: `${indicatorTop - 2}px`, height: '4px' }"
      />

      <div
        v-if="activeLabel"
        class="timeline-mobile-active-label pointer-events-none absolute z-20 rounded-full px-3 py-1 text-xs whitespace-nowrap"
        :style="{ top: `${activeLabelTop}px` }"
      >
        {{ activeLabel }}
      </div>

      <div
        v-if="hoverLabel"
        class="timeline-hover-label animate-fade-in pointer-events-none absolute -left-20 z-20 rounded-sm px-2 text-xs leading-6"
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

.timeline-scrollbar {
  --timeline-surface: color-mix(in srgb, var(--popover) 92%, transparent);
  --timeline-surface-foreground: var(--popover-foreground);
}

:global(.dark) .timeline-scrollbar {
  --timeline-surface: color-mix(in srgb, var(--popover) 84%, transparent);
}

.timeline-hover-label {
  background-color: var(--timeline-surface);
  color: var(--timeline-surface-foreground);
}

.timeline-mobile-track,
.timeline-mobile-handle,
.timeline-mobile-active-label,
.timeline-mobile-marker,
.timeline-mobile-label {
  display: none;
}

.timeline-rail-layer {
  position: absolute;
  inset: 0;
  pointer-events: none;
}

@media (max-width: 639px) {
  .timeline-scrollbar {
    position: absolute;
    top: 0;
    right: 0.25rem;
    bottom: 0;
    z-index: 30;
    width: 2.75rem;
    overflow: visible;
    touch-action: none;
    -webkit-tap-highlight-color: transparent;
    --timeline-rail-right: 0.375rem;
    --timeline-rail-width: 0.75rem;
    --timeline-thumb-surface: color-mix(
      in srgb,
      var(--timeline-surface) 82%,
      var(--foreground) 18%
    );
  }

  .timeline-scrollbar-disabled {
    pointer-events: none;
  }

  .timeline-desktop-marker,
  .timeline-desktop-position-line,
  .timeline-desktop-hover-indicator,
  .timeline-desktop-label,
  .timeline-mobile-marker,
  .timeline-mobile-label {
    display: none;
  }

  .timeline-mobile-marker,
  .timeline-mobile-label {
    display: block;
  }

  .timeline-marker,
  .timeline-label,
  .timeline-hover-indicator,
  .timeline-hover-label {
    opacity: 0;
    transition: opacity 180ms ease-out;
  }

  .timeline-scrollbar-track-active .timeline-marker,
  .timeline-scrollbar-track-active .timeline-label,
  .timeline-scrollbar-track-active .timeline-hover-indicator,
  .timeline-scrollbar-track-active .timeline-hover-label {
    opacity: 1;
  }

  .timeline-rail-layer {
    top: 0;
    right: var(--timeline-rail-right);
    bottom: 0;
    left: auto;
    width: var(--timeline-rail-width);
  }

  .timeline-marker {
    right: auto;
    left: 50%;
    transform: translateX(-50%);
  }

  .timeline-mobile-track {
    display: block;
    right: 0;
    left: 0;
    width: auto;
    background-color: var(--timeline-surface);
    opacity: 0;
    transform: scaleY(0.94);
    transition:
      opacity 180ms ease-out,
      transform 180ms ease-out;
  }

  .timeline-scrollbar-track-active .timeline-mobile-track {
    opacity: 1;
    transform: scaleY(1);
  }

  .timeline-mobile-handle {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 4px;
    right: 0;
    left: 0;
    width: auto;
    background-color: var(--timeline-thumb-surface);
  }

  .timeline-mobile-handle {
    opacity: 0;
    pointer-events: none;
    transition: opacity 300ms ease-out;
  }

  .timeline-scrollbar-handle-active .timeline-mobile-handle {
    opacity: 1;
    pointer-events: auto;
  }

  .timeline-scrollbar-track-active .timeline-label {
    right: calc(var(--timeline-rail-right) + var(--timeline-rail-width) + 0.375rem);
    left: auto;
    width: max-content;
    min-width: 2.75rem;
    border-radius: 9999px;
    background-color: var(--timeline-surface);
    color: var(--timeline-surface-foreground);
    text-align: right;
    white-space: nowrap;
  }

  .timeline-mobile-active-label {
    display: block;
    right: calc(var(--timeline-rail-right) + var(--timeline-rail-width) + 0.375rem);
    left: auto;
    background-color: var(--timeline-surface);
    color: var(--timeline-surface-foreground);
    opacity: 0;
    transform: translateX(4px);
    transition:
      opacity 180ms ease-out,
      transform 180ms ease-out;
  }

  .timeline-scrollbar-track-active .timeline-mobile-active-label {
    opacity: 1;
    pointer-events: auto;
    transform: translateX(0);
  }

  .timeline-hover-label {
    display: none;
  }
}
</style>
