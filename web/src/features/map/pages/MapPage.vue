<script setup lang="ts">
import { ref } from 'vue'

const mapIframe = ref<HTMLIFrameElement | null>(null)

const handleAddTestMarker = () => {
  if (mapIframe.value && mapIframe.value.contentWindow) {
    // 以后接收到实际坐标时，可以通过函数参数传入
    const lat = 331495
    const lng = 754601

    mapIframe.value.contentWindow.postMessage(
      {
        action: 'ADD_MARKER',
        payload: {
          lat: lat,
          lng: lng,
          popupHtml: `<div style="text-align: center;">
                        <h4 style="margin:0 0 5px 0;">测试标点</h4>
                        <p style="margin:0; font-size:12px; color:#666;">坐标: ${lat}, ${lng}</p>
                      </div>`,
        },
      },
      '*'
    )
  }
}
</script>

<template>
  <div class="relative flex h-full w-full flex-col bg-background pt-[var(--titlebar-height,20px)]">
    <!-- Header -->
    <div class="z-10 flex h-12 w-full shrink-0 items-center border-b bg-background px-4">
      <h1 class="text-base font-semibold">官方地图</h1>
      <div class="ml-auto">
        <button
          @click="handleAddTestMarker"
          class="rounded-md bg-primary px-4 py-1.5 text-sm font-medium text-primary-foreground shadow transition-colors hover:bg-primary/90"
        >
          在地图上添加测试点
        </button>
      </div>
    </div>

    <!-- Iframe Container -->
    <div class="relative w-full flex-1">
      <iframe
        ref="mapIframe"
        src="https://myl.nuanpaper.com/tools/map"
        class="absolute inset-0 h-full w-full border-none"
        allowfullscreen
      ></iframe>
    </div>
  </div>
</template>
