<script setup lang="ts">
import { ref, watch, onUnmounted } from 'vue'
import { useEventListener } from '@vueuse/core'
import { useGalleryAssetActions, useGalleryData } from '../composables'
import { useGalleryStore } from '../store'
import {
  consumeHero,
  endHeroAnimation,
  prepareReverseHero,
  consumeReverseHero,
} from '../composables/useHeroTransition'
import { galleryApi } from '../api'
import GalleryToolbar from '../components/GalleryToolbar.vue'
import GalleryContent from '../components/GalleryContent.vue'
import GalleryLightbox from '../components/lightbox/GalleryLightbox.vue'

const galleryData = useGalleryData()
const store = useGalleryStore()
const assetActions = useGalleryAssetActions()
const viewerRef = ref<HTMLElement | null>(null)
const galleryContentRef = ref<InstanceType<typeof GalleryContent> | null>(null)

// Hero overlay 动画状态
interface HeroOverlayState {
  thumbnailUrl: string
  toRect: DOMRect
}

const heroOverlay = ref<HeroOverlayState | null>(null)
const heroOverlayStyle = ref<Record<string, string>>({})
const heroActive = ref(false)
let heroRafId: number | null = null

// 反向 hero overlay 动画状态
const reverseHeroOverlay = ref<{ thumbnailUrl: string } | null>(null)
const reverseHeroOverlayStyle = ref<Record<string, string>>({})
const reverseHeroActive = ref(false)
let reverseHeroRafId: number | null = null

// lightbox 打开期间持续同步 gallery 滚动位置，确保退出时 active 卡片已在视口内
watch(
  () => store.selection.activeIndex,
  (activeIndex) => {
    if (store.lightbox.isOpen && activeIndex !== undefined) {
      galleryContentRef.value?.scrollToIndex(activeIndex)
    }
  }
)

watch(
  () => store.lightbox.isOpen,
  async (isOpen) => {
    if (!isOpen) {
      return
    }

    const hero = consumeHero()
    if (!hero) {
      return
    }

    // 从 viewer 根容器的实际 rect 计算 lightbox 内容区（toolbar 之下，filmstrip 之上）
    const TOOLBAR_HEIGHT = 61
    const FILMSTRIP_HEIGHT = store.lightbox.showFilmstrip ? 101 : 0
    const VIEWPORT_PADDING = 32
    const viewerEl = viewerRef.value
    if (!viewerEl) return
    const containerRect = viewerEl.getBoundingClientRect()
    const contentW = containerRect.width - VIEWPORT_PADDING * 2
    const contentH = containerRect.height - TOOLBAR_HEIGHT - FILMSTRIP_HEIGHT - VIEWPORT_PADDING * 2
    const imgW = hero.width
    const imgH = hero.height
    const scale = Math.min(contentW / imgW, contentH / imgH, 1)
    const targetW = imgW * scale
    const targetH = imgH * scale
    const targetX = containerRect.left + (containerRect.width - targetW) / 2
    const targetY =
      containerRect.top + TOOLBAR_HEIGHT + (contentH + VIEWPORT_PADDING * 2 - targetH) / 2

    const toRect = new DOMRect(targetX, targetY, targetW, targetH)

    heroOverlay.value = { thumbnailUrl: hero.thumbnailUrl, toRect }
    heroOverlayStyle.value = rectToFixedStyle(hero.rect, false)
    heroActive.value = false

    heroRafId = requestAnimationFrame(() => {
      heroRafId = requestAnimationFrame(() => {
        heroActive.value = true
        heroOverlayStyle.value = rectToFixedStyle(toRect, true)
      })
    })
  }
)

onUnmounted(() => {
  if (heroRafId !== null) cancelAnimationFrame(heroRafId)
  if (reverseHeroRafId !== null) cancelAnimationFrame(reverseHeroRafId)
})

function rectToFixedStyle(rect: DOMRect, animated: boolean): Record<string, string> {
  return {
    position: 'fixed',
    left: `${rect.left}px`,
    top: `${rect.top}px`,
    width: `${rect.width}px`,
    height: `${rect.height}px`,
    transition: animated ? 'all 280ms cubic-bezier(0.4, 0, 0.2, 1)' : 'none',
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

// 由 GalleryLightbox 在关闭序列中调用，短暂显示 gallery 以便测量 card rect
async function startReverseHero() {
  const rh = consumeReverseHero()
  if (!rh) return

  reverseHeroOverlay.value = { thumbnailUrl: rh.thumbnailUrl }
  reverseHeroOverlayStyle.value = rectToFixedStyle(rh.fromRect, false)
  reverseHeroActive.value = false

  reverseHeroRafId = requestAnimationFrame(() => {
    reverseHeroRafId = requestAnimationFrame(() => {
      reverseHeroActive.value = true
      reverseHeroOverlayStyle.value = rectToFixedStyle(rh.toRect, true)
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
  if (
    store.lightbox.isOpen ||
    isEditableTarget(event.target) ||
    store.selection.selectedIds.size === 0
  ) {
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
</script>

<template>
  <div ref="viewerRef" class="relative h-full">
    <!-- gallery 始终渲染，lightbox 打开时 visibility:hidden 保持布局和 virtualizer 活跃 -->
    <div class="flex h-full flex-col" :class="{ invisible: store.lightbox.isOpen }">
      <GalleryToolbar />
      <div class="flex-1 overflow-hidden">
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
