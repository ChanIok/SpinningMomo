<script setup lang="ts">
import { computed, ref, watch, onBeforeUnmount, onMounted, onUnmounted } from 'vue'
import { storeToRefs } from 'pinia'
import { useDebounceFn, useWindowSize } from '@vueuse/core'
import { on as onRpc, off as offRpc } from '@/core/rpc'
import { isLocalAccess } from '@/core/access'
import { Button } from '@/components/ui/button'
import { Split } from '@/components/ui/split'
import { useI18n } from '@/composables/useI18n'
import { X } from '@lucide/vue'
import { useGalleryData } from '../composables/useGalleryData'
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
const { t } = useI18n()
const { width: windowWidth } = useWindowSize()

watch(windowWidth, (width) => galleryStore.setWindowWidth(width), { immediate: true })

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

function syncCompactLayout(compact: boolean) {
  if (compact) {
    if (compactLayoutSnapshot.value) {
      return
    }

    compactLayoutSnapshot.value = {
      sidebarOpen: isSidebarOpen.value,
      detailsOpen: isDetailsOpen.value,
    }
    isSidebarOpen.value = false
    isDetailsOpen.value = false
    return
  }

  const snapshot = compactLayoutSnapshot.value
  if (!snapshot) {
    return
  }

  isSidebarOpen.value = snapshot.sidebarOpen
  isDetailsOpen.value = snapshot.detailsOpen
  compactLayoutSnapshot.value = null
}

watch(isCompactWindow, syncCompactLayout, { immediate: true })

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

onMounted(() => {
  onRpc('gallery.changed', galleryChangedHandler)
})

onBeforeUnmount(() => {
  syncCompactLayout(false)
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

        <Transition name="gallery-mobile-drawer">
          <div
            v-if="isSidebarOpen"
            class="absolute inset-0 z-50 flex"
            role="dialog"
            aria-modal="true"
            :aria-label="t('app.navigation.gallery')"
          >
            <button
              type="button"
              class="absolute inset-0 cursor-default bg-black/50"
              :aria-label="t('gallery.lightbox.toolbar.closeTitle')"
              @click="isSidebarOpen = false"
            />

            <aside
              class="relative z-10 flex h-full w-[88vw] max-w-[360px] flex-col border-r border-border bg-background shadow-2xl"
            >
              <div class="flex h-12 shrink-0 items-center justify-between border-b px-3">
                <span class="text-sm font-medium">{{ t('app.navigation.gallery') }}</span>
                <Button
                  variant="ghost"
                  size="icon"
                  class="h-10 w-10"
                  :aria-label="t('gallery.lightbox.toolbar.closeTitle')"
                  @click="isSidebarOpen = false"
                >
                  <X class="h-5 w-5" />
                </Button>
              </div>

              <div class="min-h-0 flex-1">
                <GallerySidebar />
              </div>
            </aside>
          </div>
        </Transition>
      </template>

      <!-- 左中右三区域布局 -->
      <Split
        v-else
        v-model:size="leftSidebarSize"
        direction="horizontal"
        :min="leftMinSize"
        :max="0.3"
        :disabled="!isSidebarOpen"
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

<style scoped>
.gallery-mobile-drawer-enter-active,
.gallery-mobile-drawer-leave-active {
  transition: opacity 180ms ease-out;
}

.gallery-mobile-drawer-enter-from,
.gallery-mobile-drawer-leave-to {
  opacity: 0;
}

.gallery-mobile-drawer-enter-active aside,
.gallery-mobile-drawer-leave-active aside {
  transition: transform 220ms cubic-bezier(0.22, 1, 0.36, 1);
}

.gallery-mobile-drawer-enter-from aside,
.gallery-mobile-drawer-leave-to aside {
  transform: translateX(-100%);
}
</style>
