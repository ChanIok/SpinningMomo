<script setup lang="ts">
import { computed, ref, watch, onMounted, onUnmounted } from 'vue'
import { useDebounceFn, useLocalStorage, useMediaQuery } from '@vueuse/core'
import { on as onRpc, off as offRpc } from '@/core/rpc'
import { Split } from '@/components/ui/split'
import { useGalleryLayout } from '../composables'
import { useGalleryData } from '../composables/useGalleryData'
import { useGalleryStore } from '../store'
import GallerySidebar from './GallerySidebar.vue'
import GalleryViewer from './GalleryViewer.vue'
import GalleryDetails from './GalleryDetails.vue'

const LEFT_MIN_SIZE = '180px'
const RIGHT_MIN_SIZE = '180px'
const LEFT_MIN_PX = 180
const RIGHT_MIN_PX = 180
const DEFAULT_LEFT_SIZE = '200px'
const DEFAULT_RIGHT_SIZE = '256px'
const COLLAPSED_SIZE = '0px'
const COLLAPSE_TRIGGER_PX = 40
const GALLERY_REFRESH_DEBOUNCE_MS = 400

// 使用布局管理
const { isSidebarOpen, isDetailsOpen, setSidebarOpen, setDetailsOpen } = useGalleryLayout()
const galleryData = useGalleryData()
const galleryStore = useGalleryStore()
const isBelowLg = useMediaQuery('(max-width: 1023px)')

let isUnmounted = false
let refreshInFlight = false
let refreshQueued = false

// Split 拖动状态持久化
const leftSidebarSize = useLocalStorage('gallery-left-sidebar-size', DEFAULT_LEFT_SIZE)
const rightDetailsSize = useLocalStorage('gallery-right-details-size', DEFAULT_RIGHT_SIZE)

// 面板展开时的宽度（用于收起后恢复）
const leftSidebarOpenSize = useLocalStorage('gallery-left-sidebar-open-size', DEFAULT_LEFT_SIZE)
const rightDetailsOpenSize = useLocalStorage('gallery-right-details-open-size', DEFAULT_RIGHT_SIZE)

type SplitSize = number | string

function parsePixelSize(size: SplitSize): number | null {
  if (typeof size !== 'string' || !size.trim().endsWith('px')) {
    return null
  }

  const value = parseFloat(size)
  return Number.isFinite(value) ? value : null
}

function normalizeOpenSize(size: SplitSize, minPx: number, fallback: string): string {
  const px = parsePixelSize(size)
  if (px === null || px <= 0) {
    return fallback
  }
  return `${Math.max(minPx, Math.round(px))}px`
}

function isAtMinSize(size: SplitSize, minPx: number): boolean {
  const px = parsePixelSize(size)
  if (px === null) {
    return false
  }
  return px <= minPx + 0.5
}

const leftMinSize = computed(() => (isSidebarOpen.value ? LEFT_MIN_SIZE : COLLAPSED_SIZE))
const rightMinSize = computed(() => (isDetailsOpen.value ? RIGHT_MIN_SIZE : COLLAPSED_SIZE))

// 仅在进入 <lg 区间时自动折叠详情面板；离开区间不自动恢复
watch(
  isBelowLg,
  (belowLg, prevBelowLg) => {
    if (belowLg && !prevBelowLg) {
      setDetailsOpen(false)
    }
  },
  { immediate: true }
)

watch(
  isSidebarOpen,
  (open) => {
    if (open) {
      const restoredSize = normalizeOpenSize(
        leftSidebarOpenSize.value,
        LEFT_MIN_PX,
        DEFAULT_LEFT_SIZE
      )
      leftSidebarSize.value = restoredSize
      leftSidebarOpenSize.value = restoredSize
      return
    }

    const currentSize = parsePixelSize(leftSidebarSize.value)
    if (currentSize !== null && currentSize > 0) {
      leftSidebarOpenSize.value = `${Math.round(currentSize)}px`
    }
    leftSidebarSize.value = COLLAPSED_SIZE
  },
  { immediate: true }
)

watch(
  isDetailsOpen,
  (open) => {
    if (open) {
      const restoredSize = normalizeOpenSize(
        rightDetailsOpenSize.value,
        RIGHT_MIN_PX,
        DEFAULT_RIGHT_SIZE
      )
      rightDetailsSize.value = restoredSize
      rightDetailsOpenSize.value = restoredSize
      return
    }

    const currentSize = parsePixelSize(rightDetailsSize.value)
    if (currentSize !== null && currentSize > 0) {
      rightDetailsOpenSize.value = `${Math.round(currentSize)}px`
    }
    rightDetailsSize.value = COLLAPSED_SIZE
  },
  { immediate: true }
)

watch(leftSidebarSize, (size) => {
  if (!isSidebarOpen.value) {
    if (size !== COLLAPSED_SIZE) {
      leftSidebarSize.value = COLLAPSED_SIZE
    }
    return
  }

  const px = parsePixelSize(size)
  if (px !== null && px >= LEFT_MIN_PX) {
    leftSidebarOpenSize.value = `${Math.round(px)}px`
  }
})

watch(rightDetailsSize, (size) => {
  if (!isDetailsOpen.value) {
    if (size !== COLLAPSED_SIZE) {
      rightDetailsSize.value = COLLAPSED_SIZE
    }
    return
  }

  const px = parsePixelSize(size)
  if (px !== null && px >= RIGHT_MIN_PX) {
    rightDetailsOpenSize.value = `${Math.round(px)}px`
  }
})

// 拖拽起点记录（用于判断“超出最小宽度阈值后收起”）
const leftDragStartX = ref<number | null>(null)
const leftDragStartSizePx = ref<number | null>(null)
const leftCollapsedByDrag = ref(false)
const rightDragStartX = ref<number | null>(null)
const rightDragStartSizePx = ref<number | null>(null)
const rightCollapsedByDrag = ref(false)

function handleLeftDragStart(e: MouseEvent) {
  leftDragStartX.value = e.clientX
  leftDragStartSizePx.value = parsePixelSize(leftSidebarSize.value)
  leftCollapsedByDrag.value = false
}

function handleLeftDrag(e: MouseEvent) {
  if (leftDragStartX.value === null || leftDragStartSizePx.value === null) {
    return
  }

  const moveToCollapseDirection = leftDragStartX.value - e.clientX
  const distanceToMin = Math.max(0, leftDragStartSizePx.value - LEFT_MIN_PX)
  const overshoot = moveToCollapseDirection - distanceToMin
  const currentDragSizePx = leftDragStartSizePx.value - moveToCollapseDirection

  if (isSidebarOpen.value) {
    if (isAtMinSize(leftSidebarSize.value, LEFT_MIN_PX) && overshoot >= COLLAPSE_TRIGGER_PX) {
      setSidebarOpen(false)
      leftCollapsedByDrag.value = true
      leftSidebarSize.value = COLLAPSED_SIZE
    }
    return
  }

  if (!leftCollapsedByDrag.value) {
    return
  }

  // 同一次拖拽中，如果回拉到收起阈值以内，自动恢复显示
  if (overshoot <= COLLAPSE_TRIGGER_PX) {
    const restoredSize = `${Math.max(LEFT_MIN_PX, Math.round(currentDragSizePx))}px`
    leftSidebarOpenSize.value = restoredSize
    setSidebarOpen(true)
    leftCollapsedByDrag.value = false
  }
}

function handleRightDragStart(e: MouseEvent) {
  rightDragStartX.value = e.clientX
  rightDragStartSizePx.value = parsePixelSize(rightDetailsSize.value)
  rightCollapsedByDrag.value = false
}

function handleRightDrag(e: MouseEvent) {
  if (rightDragStartX.value === null || rightDragStartSizePx.value === null) {
    return
  }

  const moveToCollapseDirection = e.clientX - rightDragStartX.value
  const distanceToMin = Math.max(0, rightDragStartSizePx.value - RIGHT_MIN_PX)
  const overshoot = moveToCollapseDirection - distanceToMin
  const currentDragSizePx = rightDragStartSizePx.value - moveToCollapseDirection

  if (isDetailsOpen.value) {
    if (isAtMinSize(rightDetailsSize.value, RIGHT_MIN_PX) && overshoot >= COLLAPSE_TRIGGER_PX) {
      setDetailsOpen(false)
      rightCollapsedByDrag.value = true
      rightDetailsSize.value = COLLAPSED_SIZE
    }
    return
  }

  if (!rightCollapsedByDrag.value) {
    return
  }

  // 同一次拖拽中，如果回拉到收起阈值以内，自动恢复显示
  if (overshoot <= COLLAPSE_TRIGGER_PX) {
    const restoredSize = `${Math.max(RIGHT_MIN_PX, Math.round(currentDragSizePx))}px`
    rightDetailsOpenSize.value = restoredSize
    setDetailsOpen(true)
    rightCollapsedByDrag.value = false
  }
}

function handleLeftDragEnd() {
  leftDragStartX.value = null
  leftDragStartSizePx.value = null
  leftCollapsedByDrag.value = false
}

function handleRightDragEnd() {
  rightDragStartX.value = null
  rightDragStartSizePx.value = null
  rightCollapsedByDrag.value = false
}

async function refreshGalleryFromNotification() {
  if (refreshInFlight) {
    refreshQueued = true
    return
  }

  refreshInFlight = true
  do {
    refreshQueued = false
    try {
      await galleryData.loadFolderTree()
      if (galleryStore.isTimelineMode) {
        await galleryData.loadTimelineData()
      } else {
        await galleryData.loadAllAssets()
      }
    } catch (error) {
      console.error('Failed to refresh gallery after notification:', error)
    }
  } while (refreshQueued)

  refreshInFlight = false
}

const scheduleGalleryRefresh = useDebounceFn(() => {
  if (isUnmounted) {
    return
  }
  void refreshGalleryFromNotification()
}, GALLERY_REFRESH_DEBOUNCE_MS)

const galleryChangedHandler = () => {
  void scheduleGalleryRefresh()
}

onMounted(() => {
  onRpc('gallery.changed', galleryChangedHandler)
})

onUnmounted(() => {
  isUnmounted = true
  offRpc('gallery.changed', galleryChangedHandler)
})
</script>

<template>
  <!-- 左中右三区域布局 -->
  <div class="h-full w-full border-t">
    <!-- 第一层分割：左侧 + (中右) -->
    <Split
      v-model:size="leftSidebarSize"
      direction="horizontal"
      :min="leftMinSize"
      :max="0.3"
      :disabled="!isSidebarOpen"
      @drag-start="handleLeftDragStart"
      @drag="handleLeftDrag"
      @drag-end="handleLeftDragEnd"
    >
      <!-- 左侧区域 - 侧边栏 -->
      <template #1>
        <GallerySidebar v-if="isSidebarOpen" />
      </template>

      <!-- 中右区域 -->
      <template #2>
        <!-- 第二层分割：中间 + 右侧 -->
        <Split
          v-model:size="rightDetailsSize"
          direction="horizontal"
          reverse
          :min="rightMinSize"
          :max="0.5"
          :disabled="!isDetailsOpen"
          @drag-start="handleRightDragStart"
          @drag="handleRightDrag"
          @drag-end="handleRightDragEnd"
        >
          <!-- 中间区域 - 主要内容 -->
          <template #1>
            <GalleryViewer />
          </template>

          <!-- 右侧区域 - 详情面板 -->
          <template #2>
            <GalleryDetails v-if="isDetailsOpen" />
          </template>
        </Split>
      </template>
    </Split>
  </div>
</template>
