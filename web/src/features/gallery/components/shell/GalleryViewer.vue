<script setup lang="ts">
import { ref, watch, onMounted, onUnmounted, computed } from 'vue'
import { useDebounceFn, useEventListener, usePreferredReducedMotion } from '@vueuse/core'
import { LoaderCircle, Upload, X } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { useI18n } from '@/composables/useI18n'
import {
  useGalleryAssetActions,
  useGalleryData,
  useGalleryFolderActions,
  useGallerySelection,
  useGalleryPinchZoom,
  useVisibleAssetTags,
} from '../../composables'
import { hasGalleryAssetDragIds } from '../../composables/useGalleryDragPayload'
import {
  isGalleryLightboxOverlay,
  useGalleryOverlayHistory,
} from '../../composables/useGalleryOverlayHistory'
import { useGalleryStore } from '../../store'
import { isWebView } from '@/core/env'
import { isLocalAccess } from '@/core/access'
import {
  isGalleryTouchContextMenu,
  normalizeGalleryInputType,
  type GalleryInputType,
} from '../../input'
import {
  computeLightboxHeroRect,
  consumeHero,
  endHeroAnimation,
  consumeReverseHero,
} from '../../composables/useHeroTransition'
import GalleryToolbar from './GalleryToolbar.vue'
import GalleryCompactToolbar from '../mobile/GalleryCompactToolbar.vue'
import GalleryContent from './GalleryContent.vue'
import GalleryLightbox from '../lightbox/GalleryLightbox.vue'
import GalleryPreferencesDialog from '../dialogs/GalleryPreferencesDialog.vue'
import GalleryMobileActionBar from '../mobile/GalleryMobileActionBar.vue'

const galleryData = useGalleryData()
const store = useGalleryStore()
const overlayHistory = useGalleryOverlayHistory()
const assetActions = useGalleryAssetActions()
const folderActions = useGalleryFolderActions()
const gallerySelection = useGallerySelection()
const { t } = useI18n()
const viewerRef = ref<HTMLElement | null>(null)
const galleryContentRef = ref<InstanceType<typeof GalleryContent> | null>(null)
const contentRef = ref<HTMLElement | null>(null)
const reduceMotion = usePreferredReducedMotion()
const shouldReduceMotion = computed(() => reduceMotion.value === 'reduce')
const CONTENT_WHEEL_ZOOM_THRESHOLD = 96
const isExternalDragActive = ref(false)
const isDropImporting = ref(false)
const lastInputType = ref<GalleryInputType>('mouse')
let externalDragDepth = 0
const isMultiSelectMode = computed(() => store.selection.mode === 'multi-select')

const pinchZoomEnabled = computed(() => store.isCompactWindow && !store.lightbox.isOpen)

useGalleryPinchZoom({
  surfaceRef: contentRef,
  enabled: pinchZoomEnabled,
  getSize: () => store.getEffectiveViewSize(),
  getMinSize: () => store.getViewSizeRange().min,
  getMaxSize: () => store.getViewSizeRange().max,
  setSize: (size) => store.setViewSize(size),
})

useVisibleAssetTags()

const galleryColumnClass = computed(() => {
  const hidden = store.lightbox.isOpen && !store.lightbox.isClosing
  const transition = shouldReduceMotion.value ? '' : 'transition-opacity duration-[220ms] ease-out'
  return [
    'flex h-full flex-col',
    transition,
    hidden ? 'pointer-events-none opacity-0' : 'opacity-100',
  ].filter(Boolean)
})

// Hero overlay 动画状态
interface HeroOverlayState {
  thumbnailUrl: string
  toRect: DOMRect
}

const heroOverlay = ref<HeroOverlayState | null>(null)
const heroOverlayStyle = ref<Record<string, string>>({})
const heroActive = ref(false)
let heroRafId: number | null = null
// lightbox 打开期间，gallery 背景只做低优先级“预对齐”；连续切图时只追最后一张。
let pendingGalleryScrollIndex: number | undefined
let galleryScrollRafId: number | null = null
let isViewerUnmounted = false
let wheelZoomDelta = 0
let isRestoringLightbox = false

// 清理旧版恢复参数但保留当前 history state，避免启动恢复时破坏覆盖层历史链。
function clearLightboxRecoveryParams() {
  const currentUrl = new URL(window.location.href)
  currentUrl.searchParams.delete('lbAssetId')
  currentUrl.searchParams.delete('lbFolderId')
  currentUrl.searchParams.delete('lbRetry')
  window.history.replaceState(window.history.state, '', currentUrl.toString())
}

// 按资产 ID 重建当前查询集中的选择，供前进/后退和页面恢复复用。
async function restoreLightboxAsset(assetId: number): Promise<boolean> {
  if (!Number.isInteger(assetId) || assetId <= 0) {
    return false
  }

  await galleryData.refreshCurrentQuery()
  const allAssetIds = await galleryData.queryCurrentAssetIds()
  const index = allAssetIds.findIndex((id) => id === assetId)
  if (index < 0) {
    return false
  }

  const selectedAsset = await gallerySelection.selectOnlyIndex(index)
  return Boolean(selectedAsset)
}

// 兼容外部恢复链接：先恢复筛选和资产，再创建新的暗房历史层。
async function restoreLightboxFromQuery() {
  const currentUrl = new URL(window.location.href)
  const assetIdRaw = currentUrl.searchParams.get('lbAssetId')
  const folderIdRaw = currentUrl.searchParams.get('lbFolderId')
  if (!assetIdRaw) {
    return
  }
  if (!folderIdRaw) {
    clearLightboxRecoveryParams()
    return
  }

  const assetId = Number(assetIdRaw)
  if (!Number.isInteger(assetId) || assetId <= 0) {
    clearLightboxRecoveryParams()
    return
  }

  if (folderIdRaw === 'all') {
    store.setFilter({ folderId: undefined })
  } else {
    const folderId = Number(folderIdRaw)
    if (!Number.isInteger(folderId) || folderId <= 0) {
      clearLightboxRecoveryParams()
      return
    }
    store.setFilter({ folderId: String(folderId) })
  }

  try {
    isRestoringLightbox = true
    const restored = await restoreLightboxAsset(assetId)
    if (!restored) {
      clearLightboxRecoveryParams()
      return
    }

    store.openLightbox()
    // 恢复逻辑成功后再写入覆盖层历史，避免无效资产污染返回栈。
    await overlayHistory.openLightbox(assetId)
    clearLightboxRecoveryParams()
  } catch (error) {
    console.warn('Failed to restore lightbox state:', error)
  } finally {
    isRestoringLightbox = false
  }
}

// 反向 hero overlay 动画状态
const reverseHeroOverlay = ref<{ thumbnailUrl: string } | null>(null)
const reverseHeroOverlayStyle = ref<Record<string, string>>({})
const reverseHeroActive = ref(false)
let reverseHeroRafId: number | null = null

// 吸收一小段时间内的连续 activeIndex 变化，并把背景滚动放到下一帧，避免与前景切图争抢同一拍。
const flushGalleryScrollSync = useDebounceFn(() => {
  const targetIndex = pendingGalleryScrollIndex
  if (isViewerUnmounted || !store.lightbox.isOpen || targetIndex === undefined) {
    return
  }

  if (galleryScrollRafId !== null) {
    cancelAnimationFrame(galleryScrollRafId)
  }

  galleryScrollRafId = requestAnimationFrame(() => {
    galleryScrollRafId = null
    if (isViewerUnmounted || !store.lightbox.isOpen || pendingGalleryScrollIndex !== targetIndex) {
      return
    }

    galleryContentRef.value?.scrollToIndex(targetIndex)
  })
}, 120)

// 背景 gallery 不做“逐次同步滚动”，而是 latest-wins 的预对齐。
// 目标是让退出时 active 卡片大概率已在视口内，同时尽量不打扰 lightbox 前景交互。
watch(
  () => store.selection.activeIndex,
  (activeIndex) => {
    if (store.lightbox.isOpen && activeIndex !== undefined) {
      pendingGalleryScrollIndex = activeIndex
      flushGalleryScrollSync()
    }
  }
)

watch(
  () => store.lightbox.isOpen,
  async (isOpen) => {
    if (!isOpen) {
      pendingGalleryScrollIndex = undefined
      if (galleryScrollRafId !== null) {
        cancelAnimationFrame(galleryScrollRafId)
        galleryScrollRafId = null
      }
      return
    }

    const hero = consumeHero()
    if (!hero) {
      return
    }

    const viewerEl = viewerRef.value
    if (!viewerEl) return
    const containerRect = viewerEl.getBoundingClientRect()
    const toRect = computeLightboxHeroRect(containerRect, hero.width, hero.height)

    heroOverlay.value = { thumbnailUrl: hero.thumbnailUrl, toRect }
    heroOverlayStyle.value = rectToFixedStyle(hero.rect, 'none')
    heroActive.value = false

    // 双 rAF：先让 overlay 以初始样式挂载，再在下一拍切到目标 rect，确保浏览器稳定触发 transition。
    heroRafId = requestAnimationFrame(() => {
      heroRafId = requestAnimationFrame(() => {
        heroActive.value = true
        heroOverlayStyle.value = rectToFixedStyle(toRect, 'enter')
      })
    })
  }
)

let lightboxHistoryRestoreRequest = 0

// 将浏览器历史中的暗房快照还原为 Store 选择，路由变化本身不直接操作界面组件。
async function syncLightboxFromHistory() {
  const requestId = ++lightboxHistoryRestoreRequest
  const snapshot = overlayHistory.snapshot.value
  if (!isGalleryLightboxOverlay(snapshot.overlay) || snapshot.assetId === undefined) {
    isRestoringLightbox = false
    return
  }

  // URL 仍指向同一资产时只需保持现有暗房，不重复刷新查询结果。
  if (store.lightbox.isOpen && store.selection.activeAssetId === snapshot.assetId) {
    return
  }

  try {
    isRestoringLightbox = true
    const restored = await restoreLightboxAsset(snapshot.assetId)
    if (!restored || requestId !== lightboxHistoryRestoreRequest) {
      return
    }

    const currentSnapshot = overlayHistory.snapshot.value
    if (
      !isGalleryLightboxOverlay(currentSnapshot.overlay) ||
      currentSnapshot.assetId !== snapshot.assetId
    ) {
      return
    }

    // 只有异步恢复完成且路由没有再次变化时才打开暗房，避免旧请求覆盖新状态。
    store.openLightbox()
  } catch (error) {
    console.warn('Failed to restore lightbox history state:', error)
  } finally {
    if (requestId === lightboxHistoryRestoreRequest) {
      isRestoringLightbox = false
    }
  }
}

watch(
  () => [overlayHistory.snapshot.value.overlay, overlayHistory.snapshot.value.assetId] as const,
  () => {
    void syncLightboxFromHistory()
  },
  { immediate: true }
)

// 紧凑布局中的设置也属于同页临时层；桌面端仍由原有 Dialog 状态直接控制。
watch(
  () => overlayHistory.snapshot.value.overlay,
  (overlay) => {
    if (store.isCompactWindow) {
      store.setPreferencesDialogOpen(overlay === 'preferences')
    }
  },
  { immediate: true }
)

function handlePreferencesDialogOpenChange(open: boolean) {
  if (open) {
    if (store.isCompactWindow) {
      void overlayHistory.openPreferencesPanel()
    } else {
      store.setPreferencesDialogOpen(true)
    }
    return
  }

  if (overlayHistory.snapshot.value.overlay === 'preferences') {
    void overlayHistory.closePreferencesPanel()
  } else {
    store.setPreferencesDialogOpen(false)
  }
}

// 切换图片不增加历史层级，只更新当前条目的资产身份，保证前进时回到最后查看的图片。
watch(
  () => store.selection.activeAssetId,
  (assetId) => {
    if (
      assetId === undefined ||
      !store.lightbox.isOpen ||
      !isGalleryLightboxOverlay(overlayHistory.snapshot.value.overlay)
    ) {
      return
    }

    void overlayHistory.replaceLightboxAsset(assetId)
  }
)

onMounted(async () => {
  await restoreLightboxFromQuery()
})

onUnmounted(() => {
  isViewerUnmounted = true
  store.setPreferencesDialogOpen(false)
  pendingGalleryScrollIndex = undefined
  if (heroRafId !== null) cancelAnimationFrame(heroRafId)
  if (reverseHeroRafId !== null) cancelAnimationFrame(reverseHeroRafId)
  if (galleryScrollRafId !== null) cancelAnimationFrame(galleryScrollRafId)
})

const resetWheelZoomDelta = useDebounceFn(() => {
  wheelZoomDelta = 0
}, 140)

function rectToFixedStyle(
  rect: DOMRect,
  animation: 'none' | 'enter' | 'exit'
): Record<string, string> {
  // 进入更柔和，退出更利落；这里只过渡几何属性，避免 transition: all 带来不必要的副作用。
  const transition =
    animation === 'enter'
      ? 'left 260ms cubic-bezier(0.22, 1, 0.36, 1), top 260ms cubic-bezier(0.22, 1, 0.36, 1), width 260ms cubic-bezier(0.22, 1, 0.36, 1), height 260ms cubic-bezier(0.22, 1, 0.36, 1)'
      : animation === 'exit'
        ? 'left 220ms cubic-bezier(0.4, 0, 0.2, 1), top 220ms cubic-bezier(0.4, 0, 0.2, 1), width 220ms cubic-bezier(0.4, 0, 0.2, 1), height 220ms cubic-bezier(0.4, 0, 0.2, 1)'
        : 'none'

  return {
    position: 'fixed',
    left: `${rect.left}px`,
    top: `${rect.top}px`,
    width: `${rect.width}px`,
    height: `${rect.height}px`,
    transition,
    zIndex: '9999',
    objectFit: 'cover',
    borderRadius: '4px',
    pointerEvents: 'none',
  }
}

function onHeroTransitionEnd() {
  heroOverlay.value = null
  heroActive.value = false
  endHeroAnimation()
}

function onReverseHeroTransitionEnd() {
  reverseHeroOverlay.value = null
  reverseHeroActive.value = false
}

// 由 GalleryLightbox 在关闭序列中触发反向 hero 飞回
async function startReverseHero() {
  const rh = consumeReverseHero()
  if (!rh) return

  reverseHeroOverlay.value = { thumbnailUrl: rh.thumbnailUrl }
  reverseHeroOverlayStyle.value = rectToFixedStyle(rh.fromRect, 'none')
  reverseHeroActive.value = false

  reverseHeroRafId = requestAnimationFrame(() => {
    reverseHeroRafId = requestAnimationFrame(() => {
      reverseHeroActive.value = true
      reverseHeroOverlayStyle.value = rectToFixedStyle(rh.toRect, 'exit')
    })
  })
}

defineExpose({ startReverseHero })

function toggleSelectedAssetsRejected() {
  const activeIndex = store.selection.activeIndex
  const activeAsset =
    activeIndex === undefined ? null : (store.getAssetsInRange(activeIndex, activeIndex)[0] ?? null)

  if (activeAsset?.reviewFlag === 'rejected') {
    void assetActions.clearSelectedAssetsRejected()
    return
  }

  void assetActions.setSelectedAssetsRejected()
}

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false
  }

  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName)
}

function handleKeydown(event: KeyboardEvent) {
  if (store.lightbox.isOpen || isEditableTarget(event.target)) {
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.shiftKey) {
    const key = event.key.toLowerCase()
    if (key === 'c') {
      event.preventDefault()
      void assetActions.copySelectedAssetTags()
      return
    }
    if (key === 'v') {
      event.preventDefault()
      void assetActions.pasteCopiedTagsToSelection()
      return
    }
  }

  if (
    (event.ctrlKey || event.metaKey) &&
    !event.shiftKey &&
    // 普通系统剪贴板只属于本机；LAN 下保留浏览器默认快捷键行为。
    isLocalAccess() &&
    event.key.toLowerCase() === 'c' &&
    store.selection.selectedIds.size > 0
  ) {
    event.preventDefault()
    void assetActions.handleCopyAssetsToClipboard()
    return
  }

  // 普通 Ctrl+V 只导入到当前明确选中的文件夹，不占用标签粘贴快捷键。
  // LAN 下不拦截默认粘贴，避免远端页面弹出本机 Clipboard API 错误。
  if ((event.ctrlKey || event.metaKey) && isLocalAccess() && event.key.toLowerCase() === 'v') {
    event.preventDefault()
    void folderActions.pasteClipboardToSelectedFolder()
    return
  }

  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'a') {
    event.preventDefault()
    void gallerySelection.selectAllCurrentQuery()
    return
  }

  if (store.selection.selectedIds.size === 0) {
    return
  }

  switch (event.key) {
    case '0':
      event.preventDefault()
      void assetActions.clearSelectedAssetsRating()
      return
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
      event.preventDefault()
      void assetActions.setSelectedAssetsRating(Number(event.key))
      return
    case 'x':
    case 'X':
      event.preventDefault()
      toggleSelectedAssetsRejected()
      return
  }
}

function handleContentWheel(event: WheelEvent) {
  if (store.lightbox.isOpen || !event.ctrlKey || isEditableTarget(event.target)) {
    return
  }

  event.preventDefault()

  if (event.deltaY === 0) {
    return
  }

  wheelZoomDelta += event.deltaY
  resetWheelZoomDelta()

  while (Math.abs(wheelZoomDelta) >= CONTENT_WHEEL_ZOOM_THRESHOLD) {
    if (wheelZoomDelta > 0) {
      store.decreaseViewSize()
      wheelZoomDelta -= CONTENT_WHEEL_ZOOM_THRESHOLD
      continue
    }

    store.increaseViewSize()
    wheelZoomDelta += CONTENT_WHEEL_ZOOM_THRESHOLD
  }
}

function handleContentContextMenu(event: MouseEvent) {
  if (isGalleryTouchContextMenu(event, lastInputType.value)) {
    event.preventDefault()
    event.stopPropagation()
    return
  }

  if (isEditableTarget(event.target)) {
    return
  }

  // 背景右键切换到当前文件夹上下文，与素材右键的选区动作明确分离。
  gallerySelection.clearSelection()
  store.openContextMenuForBackground(event)
}

function handleContentPointerDown(event: PointerEvent) {
  if (event.isPrimary !== false) {
    lastInputType.value = normalizeGalleryInputType(event.pointerType)
  }
}

// 区分资源管理器文件与图库内部资产拖拽，避免两套 drop 语义互相抢占。
function isExternalFileDrag(event: DragEvent): boolean {
  if (hasGalleryAssetDragIds(event)) {
    return false
  }
  return Array.from(event.dataTransfer?.types ?? []).includes('Files')
}

// 清空嵌套 dragenter/dragleave 计数并关闭导入覆盖层。
function resetExternalDragState() {
  externalDragDepth = 0
  isExternalDragActive.value = false
}

// 只有 HWND WebView 的普通图库视图绑定了唯一文件夹时才接受外部文件。
function canImportExternalFiles(): boolean {
  // 外部文件导入要求本机 WebView、唯一目标文件夹且当前没有其他导入任务。
  return (
    isLocalAccess() &&
    isWebView() &&
    !store.lightbox.isOpen &&
    !isDropImporting.value &&
    folderActions.selectedFolderId.value !== undefined
  )
}

// 外部文件进入内容区时阻止默认导航，并按当前目标状态显示复制反馈。
function handleViewerDragEnter(event: DragEvent) {
  if (!isExternalFileDrag(event)) {
    return
  }

  // 即使没有明确目标也阻止 WebView 导航打开被拖入的文件。
  event.preventDefault()
  externalDragDepth += 1
  isExternalDragActive.value = canImportExternalFiles()
  if (event.dataTransfer) {
    event.dataTransfer.dropEffect = isExternalDragActive.value ? 'copy' : 'none'
  }
}

// 拖拽经过内容区时分别维持外部复制和内部移动的鼠标反馈。
function handleViewerDragOver(event: DragEvent) {
  if (isExternalFileDrag(event)) {
    // 文件拖拽期间持续重算目标，支持用户尚未松手时切换图库状态。
    event.preventDefault()
    isExternalDragActive.value = canImportExternalFiles()
    if (event.dataTransfer) {
      event.dataTransfer.dropEffect = isExternalDragActive.value ? 'copy' : 'none'
    }
    return
  }

  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  // 让 viewer 区域在拖拽经过时保持“可移动”手势，避免系统显示禁止图标。
  event.preventDefault()
  if (event.dataTransfer) {
    event.dataTransfer.dropEffect = 'move'
  }
}

// 外部文件完全离开内容区后关闭覆盖层，子元素间移动不会造成闪烁。
function handleViewerDragLeave(event: DragEvent) {
  if (!isExternalFileDrag(event)) {
    return
  }
  externalDragDepth = Math.max(0, externalDragDepth - 1)
  if (externalDragDepth === 0) {
    isExternalDragActive.value = false
  }
}

// 松开外部文件后锁定目标文件夹并等待整批导入完成。
async function handleViewerDrop(event: DragEvent) {
  if (isExternalFileDrag(event)) {
    // drop 时先快照 File 对象和目标，随后清理拖拽态，避免异步期间继续响应悬停。
    event.preventDefault()
    const files = Array.from(event.dataTransfer?.files ?? [])
    const targetFolderId = folderActions.selectedFolderId.value
    resetExternalDragState()

    // 浏览器、LAN、灯箱和重复提交都只消费 drop，不启动新的导入批次。
    if (!isLocalAccess() || !isWebView() || store.lightbox.isOpen || isDropImporting.value) {
      return
    }
    if (targetFolderId === undefined) {
      folderActions.warnDropTargetRequired()
      return
    }
    if (files.length === 0) {
      return
    }

    // 导入期间保持进度覆盖层，finally 确保异常也能恢复交互。
    isDropImporting.value = true
    try {
      await folderActions.importDroppedFilesToFolder(targetFolderId, files)
    } finally {
      isDropImporting.value = false
    }
    return
  }

  if (!hasGalleryAssetDragIds(event)) {
    return
  }
  // viewer 本身不执行移动，仅消费默认 drop 行为以维持一致交互反馈。
  event.preventDefault()
}

// 监听筛选条件和文件夹选项变化，自动重新加载资产
watch(
  () => [store.filter, store.includeSubfolders, store.sortBy, store.sortOrder],
  async () => {
    if (isRestoringLightbox) {
      return
    }
    console.log('🔄 筛选条件变化，重新加载数据')
    await galleryData.refreshCurrentQuery()
  },
  { deep: true }
)

useEventListener(window, 'keydown', handleKeydown)
useEventListener(contentRef, 'wheel', handleContentWheel, { passive: false })
</script>

<template>
  <div ref="viewerRef" class="relative h-full">
    <!-- gallery 始终渲染；打开时用 opacity 隐藏以便过渡，关闭阶段 isClosing 时与 lightbox 同步淡入 -->
    <div
      :class="galleryColumnClass"
      :aria-hidden="store.lightbox.isOpen && !store.lightbox.isClosing ? true : undefined"
    >
      <!-- 紧凑窗口下工具栏浮动层叠在顶部，不占用滚动区域高度 -->
      <div v-if="store.isCompactWindow" class="absolute top-0 right-0 left-0 z-20">
        <GalleryCompactToolbar />
      </div>

      <!-- 桌面端工具栏与多选工具栏 -->
      <template v-else>
        <GalleryToolbar
          v-if="!isMultiSelectMode"
          @open-preferences="store.setPreferencesDialogOpen(true)"
        />
        <div
          v-else
          class="flex min-h-10 shrink-0 items-center justify-between border-b border-border/60 px-2"
        >
          <Button
            variant="ghost"
            size="icon"
            class="h-10 w-10"
            :aria-label="t('gallery.mobile.selection.exit')"
            @click="gallerySelection.exitMultiSelectMode"
          >
            <X class="size-5" />
          </Button>
          <span class="text-sm font-medium">
            <template v-if="store.selectedCount > 0">
              {{ t('gallery.mobile.selection.selectedCount', { count: store.selectedCount }) }}
            </template>
            <template v-else>
              {{ t('gallery.mobile.selection.empty') }}
            </template>
          </span>
          <div class="h-10 w-10" aria-hidden="true" />
        </div>
      </template>
      <div
        ref="contentRef"
        class="relative flex min-h-0 flex-1 flex-col overflow-hidden"
        :class="store.isCompactWindow && 'gallery-compact-touch-surface'"
        @pointerdown="handleContentPointerDown"
        @contextmenu="handleContentContextMenu"
        @dragenter="handleViewerDragEnter"
        @dragover="handleViewerDragOver"
        @dragleave="handleViewerDragLeave"
        @drop="handleViewerDrop"
      >
        <div class="relative min-h-0 flex-1">
          <GalleryContent ref="galleryContentRef" />

          <div
            v-if="isExternalDragActive || isDropImporting"
            class="pointer-events-none absolute inset-3 z-50 flex items-center justify-center rounded-xl border-2 border-dashed border-primary/70 bg-background/88 shadow-2xl backdrop-blur-md"
          >
            <div class="flex max-w-md flex-col items-center gap-3 px-8 text-center">
              <LoaderCircle v-if="isDropImporting" class="size-10 animate-spin text-primary" />
              <Upload v-else class="size-10 text-primary" />
              <div class="text-lg font-semibold">
                {{
                  isDropImporting
                    ? t('gallery.drop.overlayImporting')
                    : t('gallery.drop.overlayTitle')
                }}
              </div>
              <div v-if="!isDropImporting" class="text-sm text-muted-foreground">
                {{ t('gallery.drop.overlayDescription') }}
              </div>
            </div>
          </div>
        </div>

        <GalleryMobileActionBar v-if="isMultiSelectMode" />
      </div>
    </div>

    <!-- lightbox 按需挂载/销毁，绝对定位覆盖在 gallery 上层 -->
    <GalleryLightbox
      v-if="store.lightbox.isOpen"
      :gallery-content-ref="galleryContentRef"
      @request-reverse-hero="startReverseHero"
    />

    <GalleryPreferencesDialog
      :open="store.preferencesDialogOpen"
      @update:open="handlePreferencesDialogOpenChange"
    />

    <!-- Hero overlay: 缩略图放大到 lightbox 的动画层 -->
    <Teleport to="body">
      <img
        v-if="heroOverlay"
        :src="heroOverlay.thumbnailUrl"
        :style="heroOverlayStyle"
        alt=""
        @transitionend="onHeroTransitionEnd"
      />
      <img
        v-if="reverseHeroOverlay"
        :src="reverseHeroOverlay.thumbnailUrl"
        :style="reverseHeroOverlayStyle"
        alt=""
        @transitionend="onReverseHeroTransitionEnd"
      />
    </Teleport>
  </div>
</template>

<style scoped>
/* 单指保留原生纵向滚动，双指由 useGalleryPinchZoom 接管为图库尺寸调整。 */
.gallery-compact-touch-surface {
  touch-action: pan-y;
}
</style>
