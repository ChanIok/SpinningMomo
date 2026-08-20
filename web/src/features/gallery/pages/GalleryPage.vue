<script setup lang="ts">
import { computed, ref, watch, onBeforeUnmount, onMounted, onUnmounted } from 'vue'
import { storeToRefs } from 'pinia'
import { useDebounceFn, useEventListener, useWindowSize } from '@vueuse/core'
import { on as onRpc, off as offRpc } from '@/core/rpc'
import { isLocalAccess } from '@/core/access'
import { MobileDrawer } from '@/components/ui/mobile-drawer'
import { Split } from '@/components/ui/split'
import { useGalleryData } from '../composables/useGalleryData'
import { useGallerySplitPresentation } from '../composables/useGallerySplitPresentation'
import {
  isGalleryLightboxOverlay,
  useGalleryOverlayHistory,
} from '../composables/useGalleryOverlayHistory'
import { normalizeGalleryInputType } from '../input'
import { useGalleryStore } from '../store'
import { useSettingsStore } from '@/features/settings/store'
import GallerySidebar from '../components/shell/GallerySidebar.vue'
import GalleryViewer from '../components/shell/GalleryViewer.vue'
import GalleryDetails from '../components/shell/GalleryDetails.vue'
import InfinityNikkiGuidePanel from '../components/infinity_nikki/InfinityNikkiGuidePanel.vue'

const LEFT_MIN_SIZE = '180px'
const RIGHT_MIN_SIZE = '180px'
const LEFT_MIN_PX = 180
const RIGHT_MIN_PX = 180
const COLLAPSED_SIZE = '0px'
const COLLAPSE_TRIGGER_PX = 40
const GALLERY_REFRESH_DEBOUNCE_MS = 400

const galleryStore = useGalleryStore()
const overlayHistory = useGalleryOverlayHistory()
const {
  isCompactWindow,
  sidebarOpen: isSidebarOpen,
  detailsOpen: isDetailsOpen,
  leftSidebarSize,
  rightDetailsSize,
  leftSidebarOpenSize,
  rightDetailsOpenSize,
} = storeToRefs(galleryStore)
const galleryData = useGalleryData()
const settingsStore = useSettingsStore()
const { width: windowWidth } = useWindowSize()
const { showHandle: showSplitHandle, dividerSize: splitDividerSize } = useGallerySplitPresentation()

watch(windowWidth, (width) => galleryStore.setWindowWidth(width), { immediate: true })

function handleGalleryPointerDown(event: PointerEvent) {
  if (event.isPrimary === false) {
    return
  }

  galleryStore.setRecentInputType(normalizeGalleryInputType(event.pointerType))
}

function handleGalleryKeydown() {
  galleryStore.setRecentInputType('keyboard')
}

// 在图库根页面统一记录最近一次输入，覆盖桌面三栏、移动抽屉和 Teleport 内容。
useEventListener(window, 'pointerdown', handleGalleryPointerDown, { capture: true })
useEventListener(window, 'keydown', handleGalleryKeydown, { capture: true })

// 引导面板显示条件（无限暖暖拓展已启用、配置了游戏目录、且尚未看过引导）
const showInfinityNikkiGuide = computed(() => {
  const config = settingsStore.appSettings.extensions.infinityNikki
  // 引导会配置本机游戏目录，因此 LAN 页面不显示。
  return (
    isLocalAccess() && config.enable && Boolean(config.gameDir.trim()) && !config.galleryGuideSeen
  )
})

let isUnmounted = false
let refreshInFlight = false
let refreshQueued = false

type SplitSize = number | string
type SplitDragEvent = MouseEvent | TouchEvent

const compactLayoutSnapshot = ref<{ sidebarOpen: boolean; detailsOpen: boolean } | null>(null)

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

function getSplitClientX(event: SplitDragEvent): number | undefined {
  if ('touches' in event) {
    return event.touches[0]?.clientX ?? event.changedTouches[0]?.clientX
  }

  return event.clientX
}

// 在窗口模式切换时保存/恢复桌面布局，同时让紧凑窗口的文件夹历史成为显隐来源。
function syncCompactLayout(compact: boolean) {
  if (compact) {
    if (compactLayoutSnapshot.value) {
      return
    }

    compactLayoutSnapshot.value = {
      sidebarOpen: isSidebarOpen.value,
      detailsOpen: isDetailsOpen.value,
    }
    // 移动端文件夹抽屉由 URL 历史控制，桌面端原本打开的侧栏只作为布局快照保留。
    isSidebarOpen.value = overlayHistory.snapshot.value.overlay === 'folder'
    isDetailsOpen.value = false
    return
  }

  const compactOverlay = overlayHistory.snapshot.value.overlay
  if (
    compactOverlay === 'filter' ||
    compactOverlay === 'view-settings' ||
    compactOverlay === 'preferences' ||
    compactOverlay === 'selection'
  ) {
    // 紧凑布局专属面板不能带入桌面布局，避免切换窗口宽度后留下不可见历史层。
    if (compactOverlay === 'preferences') {
      galleryStore.setPreferencesDialogOpen(false)
    }
    void overlayHistory.closeTopOverlay()
  }

  const snapshot = compactLayoutSnapshot.value
  if (!snapshot) {
    return
  }

  // 离开紧凑布局时恢复桌面状态；若期间停留在 folder 历史层，优先保持抽屉打开。
  isSidebarOpen.value = snapshot.sidebarOpen || overlayHistory.snapshot.value.overlay === 'folder'
  isDetailsOpen.value = snapshot.detailsOpen
  compactLayoutSnapshot.value = null
}

watch(isCompactWindow, syncCompactLayout, { immediate: true })

// 文件夹抽屉的可见性由同页历史状态驱动；初始化和桌面布局变化不创建历史条目。
watch(
  () => overlayHistory.snapshot.value.overlay,
  (overlay, previousOverlay) => {
    const isFolderOverlay = overlay === 'folder'
    const wasFolderOverlay = previousOverlay === 'folder'
    if (!isFolderOverlay && !wasFolderOverlay) {
      return
    }

    if (isSidebarOpen.value !== isFolderOverlay) {
      isSidebarOpen.value = isFolderOverlay
    }
  },
  { immediate: true }
)

// 多选通过浏览器返回退出时，URL 是状态源；页面按钮则会先清 Store 再消费这层历史。
watch(
  () => overlayHistory.snapshot.value.overlay,
  (overlay, previousOverlay) => {
    if (
      previousOverlay === 'selection' &&
      overlay !== 'selection' &&
      galleryStore.selection.mode === 'multi-select'
    ) {
      galleryStore.exitMultiSelectMode()
    }
  }
)

const leftMinSize = computed(() => (isSidebarOpen.value ? LEFT_MIN_SIZE : COLLAPSED_SIZE))
const rightMinSize = computed(() => (isDetailsOpen.value ? RIGHT_MIN_SIZE : COLLAPSED_SIZE))

watch(
  isSidebarOpen,
  (open) => {
    if (open) {
      const restoredSize = normalizeOpenSize(leftSidebarOpenSize.value, LEFT_MIN_PX, '200px')
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
      const restoredSize = normalizeOpenSize(rightDetailsOpenSize.value, RIGHT_MIN_PX, '256px')
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

function handleLeftDragStart(e: SplitDragEvent) {
  const clientX = getSplitClientX(e)
  if (clientX === undefined) return

  leftDragStartX.value = clientX
  leftDragStartSizePx.value = parsePixelSize(leftSidebarSize.value)
  leftCollapsedByDrag.value = false
}

function handleLeftDrag(e: SplitDragEvent) {
  if (leftDragStartX.value === null || leftDragStartSizePx.value === null) {
    return
  }

  const clientX = getSplitClientX(e)
  if (clientX === undefined) return

  const moveToCollapseDirection = leftDragStartX.value - clientX
  const distanceToMin = Math.max(0, leftDragStartSizePx.value - LEFT_MIN_PX)
  const overshoot = moveToCollapseDirection - distanceToMin
  const currentDragSizePx = leftDragStartSizePx.value - moveToCollapseDirection

  if (isSidebarOpen.value) {
    if (isAtMinSize(leftSidebarSize.value, LEFT_MIN_PX) && overshoot >= COLLAPSE_TRIGGER_PX) {
      isSidebarOpen.value = false
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
    isSidebarOpen.value = true
    leftCollapsedByDrag.value = false
  }
}

function handleRightDragStart(e: SplitDragEvent) {
  const clientX = getSplitClientX(e)
  if (clientX === undefined) return

  rightDragStartX.value = clientX
  rightDragStartSizePx.value = parsePixelSize(rightDetailsSize.value)
  rightCollapsedByDrag.value = false
}

function handleRightDrag(e: SplitDragEvent) {
  if (rightDragStartX.value === null || rightDragStartSizePx.value === null) {
    return
  }

  const clientX = getSplitClientX(e)
  if (clientX === undefined) return

  const moveToCollapseDirection = clientX - rightDragStartX.value
  const distanceToMin = Math.max(0, rightDragStartSizePx.value - RIGHT_MIN_PX)
  const overshoot = moveToCollapseDirection - distanceToMin
  const currentDragSizePx = rightDragStartSizePx.value - moveToCollapseDirection

  if (isDetailsOpen.value) {
    if (isAtMinSize(rightDetailsSize.value, RIGHT_MIN_PX) && overshoot >= COLLAPSE_TRIGGER_PX) {
      isDetailsOpen.value = false
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
    isDetailsOpen.value = true
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

// 抽屉关闭必须消费历史条目，不能只修改 store，否则系统返回会再次命中同一层。
function closeFolderDrawer() {
  void overlayHistory.closeFolderDrawer()
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
      await galleryData.refreshCurrentQuery()
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

function resetGalleryInteraction() {
  galleryStore.setRecentInputType('mouse')
  galleryStore.exitMultiSelectMode()
  galleryStore.clearSelection()
  galleryStore.clearActiveAsset()
  galleryStore.clearDetailsFocus()
}

onMounted(() => {
  // 普通进入图库时从干净的浏览态开始；暗房直链由 Viewer 自己恢复资产位置。
  if (!isGalleryLightboxOverlay(overlayHistory.snapshot.value.overlay)) {
    resetGalleryInteraction()
  }
  onRpc('gallery.changed', galleryChangedHandler)
})

onBeforeUnmount(() => {
  syncCompactLayout(false)
  resetGalleryInteraction()
})

onUnmounted(() => {
  isUnmounted = true
  offRpc('gallery.changed', galleryChangedHandler)
})
</script>

<template>
  <div class="h-full w-full" :class="isCompactWindow ? 'p-0' : 'p-1 pt-0'">
    <div class="relative h-full w-full overflow-hidden" :class="[!isCompactWindow && 'rounded-sm']">
      <!-- 引导面板：占满整个画廊区域，隐藏三栏布局 -->
      <InfinityNikkiGuidePanel v-if="showInfinityNikkiGuide" />

      <!-- 窄屏布局：图库保持全宽，侧栏以覆盖式抽屉打开。 -->
      <template v-else-if="isCompactWindow">
        <GalleryViewer />

        <MobileDrawer
          :open="isSidebarOpen"
          side="left"
          class="w-[80vw] max-w-[360px]"
          @close="closeFolderDrawer"
        >
          <GallerySidebar />
        </MobileDrawer>
      </template>

      <!-- 左中右三区域布局 -->
      <Split
        v-else
        v-model:size="leftSidebarSize"
        direction="horizontal"
        :min="leftMinSize"
        :max="0.3"
        :disabled="!isSidebarOpen"
        :divider-size="splitDividerSize"
        :show-handle="showSplitHandle"
        divider-line-class="bg-transparent"
        pane1-class="surface-middle"
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
            :divider-size="splitDividerSize"
            :show-handle="showSplitHandle"
            divider-line-class="bg-transparent"
            pane1-class="surface-middle [--surface-opacity-scale:0.56]"
            pane2-class="surface-middle"
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
  </div>
</template>
