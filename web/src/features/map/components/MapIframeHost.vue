<script setup lang="ts">
// 挂在 ContentArea、与 router-view 平级：离开 /map 不销毁 iframe，避免每次重进地图冷启动
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import { MAP_URL } from '@/features/map/bridge/protocol'
import { useMapBridge } from '@/features/map/composables/useMapBridge'
import {
  flushMapRuntimeToIframe,
  registerMapIframeFlush,
} from '@/features/map/composables/mapIframeRuntime'
import { normalizeOfficialWorldId } from '@/features/map/domain/officialWorldId'
import { useMapStore } from '@/features/map/store'

const route = useRoute()
const router = useRouter()
const galleryStore = useGalleryStore()
const mapStore = useMapStore()
const mapIframe = ref<HTMLIFrameElement | null>(null)
const isMapRoute = computed(() => route.name === 'map')
/** 是否已首次进入过 /map；为 true 后 iframe 常驻 DOM，仅用 v-show 隐藏 */
const hostActivated = ref(false)
/** 官方地图页 URL；变更会触发 iframe 导航，须同步 resetIframeSession */
const mapIframeSrc = ref<string>()

const { postRuntimeSync, handleMapMessage } = useMapBridge({
  mapIframe,
  mapStore,
  galleryStore,
  router,
})

function mapSrcWithWorld(worldId?: string) {
  const id = normalizeOfficialWorldId(worldId)
  if (!id) return MAP_URL
  const url = new URL(MAP_URL)
  url.searchParams.set('worldId', id)
  return url.toString()
}

function applyPendingFocusRequest() {
  const target = mapStore.pendingFocusRequest
  if (!target) return

  const nextWorldId = normalizeOfficialWorldId(target.worldId)
  const currentWorldId = normalizeOfficialWorldId(mapStore.runtimeOptions.currentWorldId)

  // 目标区域与当前不一致：改 src 让 iframe 换 world，待 SESSION_READY 后再消费 pending
  if (nextWorldId && nextWorldId !== currentWorldId) {
    // 图库「在地图中打开」会先写 pending 再 push；未进 /map 前不能挂 iframe
    if (!hostActivated.value) return
    const nextSrc = mapSrcWithWorld(nextWorldId)
    if (mapIframeSrc.value !== nextSrc) mapIframeSrc.value = nextSrc
    return
  }

  mapStore.consumePendingFocusRequest()
  mapStore.patchRuntimeOptions({
    focusedAssetId: target.assetId,
    focusRequestId: target.requestId,
    markersVisible: true,
    currentWorldId: nextWorldId ?? currentWorldId,
  })
  if (mapStore.iframeSessionReady) flushMapRuntimeToIframe()
}

watch(
  () => [
    mapStore.pendingFocusRequest,
    mapStore.iframeSessionReady,
    mapStore.runtimeOptions.currentWorldId,
  ],
  applyPendingFocusRequest
)

// src 变化 = 子页重载，宿主侧重置 session 与标点，等 iframe 再次 SESSION_READY
watch(mapIframeSrc, (next, prev) => {
  if (next === prev) return
  mapStore.resetIframeSession()
  mapStore.replaceMarkers([])
  mapStore.patchRuntimeOptions({ currentWorldId: undefined })
})

watch(
  isMapRoute,
  (visible) => {
    if (!visible) return
    // 懒加载：首次进地图才创建 iframe；若有 pending 则初始 src 带 worldId
    if (!hostActivated.value) {
      hostActivated.value = true
      mapIframeSrc.value = mapSrcWithWorld(mapStore.pendingFocusRequest?.worldId)
    }
    // 从其他路由回到地图且 session 仍在：补一帧 runtime 同步
    if (mapStore.iframeSessionReady) flushMapRuntimeToIframe()
  },
  { immediate: true }
)

onMounted(() => {
  registerMapIframeFlush(postRuntimeSync)
  window.addEventListener('message', handleMapMessage)
})

onUnmounted(() => window.removeEventListener('message', handleMapMessage))
</script>

<template>
  <!-- v-if：未访问过地图不占第三方 origin 内存；v-show：切走路由不卸 iframe -->
  <div v-if="hostActivated" v-show="isMapRoute" class="absolute inset-x-0 top-0 bottom-0 z-0">
    <iframe
      ref="mapIframe"
      :src="mapIframeSrc!"
      class="absolute inset-0 h-full w-full border-none"
      allowfullscreen
    />
  </div>
</template>
