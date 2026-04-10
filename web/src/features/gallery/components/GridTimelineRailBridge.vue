<script setup lang="ts">
import { ref, watch, onUnmounted } from 'vue'
import type { GalleryScrollbarLabel, GalleryScrollbarMarker } from './GalleryScrollbarRail.vue'
import GalleryScrollbarRail from './GalleryScrollbarRail.vue'

interface VirtualizerLike {
  getTotalSize: () => number
  scrollToOffset: (offset: number, options?: { behavior?: 'auto' | 'smooth' }) => void
}

const props = defineProps<{
  scrollContainer: HTMLElement | null
  containerHeight: number
  virtualizer: VirtualizerLike
  markers: GalleryScrollbarMarker[]
  labels: GalleryScrollbarLabel[]
}>()

const scrollTop = ref(0)
const viewportHeight = ref(0)
let cleanup: (() => void) | null = null
let pendingFrameId: number | null = null
let pendingContainer: HTMLElement | null = null

function syncFromContainer(container: HTMLElement) {
  scrollTop.value = container.scrollTop
  viewportHeight.value = container.clientHeight
}

function flushScrollSync() {
  pendingFrameId = null
  if (!pendingContainer) {
    return
  }

  syncFromContainer(pendingContainer)
}

function scheduleScrollSync(container: HTMLElement) {
  pendingContainer = container
  if (pendingFrameId !== null) {
    return
  }

  pendingFrameId = requestAnimationFrame(flushScrollSync)
}

function detach() {
  pendingContainer = null
  if (pendingFrameId !== null) {
    cancelAnimationFrame(pendingFrameId)
    pendingFrameId = null
  }

  if (cleanup) {
    cleanup()
    cleanup = null
  }
}

function attach(container: HTMLElement) {
  const onScroll = () => {
    scheduleScrollSync(container)
  }

  syncFromContainer(container)
  container.addEventListener('scroll', onScroll, { passive: true })
  cleanup = () => {
    container.removeEventListener('scroll', onScroll)
  }
}

watch(
  () => props.scrollContainer,
  (container) => {
    detach()
    if (container) {
      attach(container)
    } else {
      scrollTop.value = 0
      viewportHeight.value = 0
    }
  },
  { immediate: true }
)

onUnmounted(() => {
  detach()
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
