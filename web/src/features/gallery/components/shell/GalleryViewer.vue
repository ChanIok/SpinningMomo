<script setup lang="ts">
import { ref, watch, onUnmounted, computed } from 'vue'
import { useDebounceFn, useEventListener, usePreferredReducedMotion } from '@vueuse/core'
import {
  useGalleryAssetActions,
  useGalleryData,
  useGallerySelection,
  useGalleryView,
} from '../../composables'
import { useGalleryStore } from '../../store'
import {
  computeLightboxHeroRect,
  consumeHero,
  endHeroAnimation,
  consumeReverseHero,
} from '../../composables/useHeroTransition'
import GalleryToolbar from './GalleryToolbar.vue'
import GalleryContent from './GalleryContent.vue'
import GalleryLightbox from '../lightbox/GalleryLightbox.vue'

const galleryData = useGalleryData()
const store = useGalleryStore()
const assetActions = useGalleryAssetActions()
const gallerySelection = useGallerySelection()
const galleryView = useGalleryView()
const viewerRef = ref<HTMLElement | null>(null)
const galleryContentRef = ref<InstanceType<typeof GalleryContent> | null>(null)
const contentRef = ref<HTMLElement | null>(null)
const reduceMotion = usePreferredReducedMotion()
const shouldReduceMotion = computed(() => reduceMotion.value === 'reduce')
const CONTENT_WHEEL_ZOOM_THRESHOLD = 96

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
    const toRect = computeLightboxHeroRect(
      containerRect,
      hero.width,
      hero.height,
      store.lightbox.showFilmstrip
    )

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

onUnmounted(() => {
  isViewerUnmounted = true
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
      galleryView.decreaseSize()
      wheelZoomDelta -= CONTENT_WHEEL_ZOOM_THRESHOLD
      continue
    }

    galleryView.increaseSize()
    wheelZoomDelta += CONTENT_WHEEL_ZOOM_THRESHOLD
  }
}

// 监听筛选条件和文件夹选项变化，自动重新加载资产
watch(
  () => [store.filter, store.includeSubfolders, store.sortBy, store.sortOrder],
  async () => {
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
      <GalleryToolbar />
      <div ref="contentRef" class="flex-1 overflow-hidden">
        <GalleryContent ref="galleryContentRef" />
      </div>
    </div>

    <!-- lightbox 按需挂载/销毁，绝对定位覆盖在 gallery 上层 -->
    <GalleryLightbox
      v-if="store.lightbox.isOpen"
      :gallery-content-ref="galleryContentRef"
      @request-reverse-hero="startReverseHero"
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
