<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { useGalleryStore } from '@/features/gallery/store'
import { useSettingsStore } from '@/features/settings/store'
import { resolveBackgroundImageUrl } from '@/features/settings/backgroundPath'
import { isWebView } from '@/core/env'
import { Toaster } from '@/components/ui/sonner'
import AppHeader from './AppHeader.vue'
import ContentArea from './ContentArea.vue'
import GalleryDebugOverlay from './GalleryDebugOverlay.vue'
import WindowResizeOverlay from './WindowResizeOverlay.vue'
import 'vue-sonner/style.css'

const route = useRoute()
const galleryStore = useGalleryStore()
const settingsStore = useSettingsStore()
const isDev = import.meta.env.DEV
const isWelcome = computed(() => route.name === 'welcome')
const isHome = computed(() => route.name === 'home')
const isGallery = computed(() => route.name === 'gallery')
const shouldHideAppHeader = computed(() => {
  if (isGallery.value && galleryStore.isCompactWindow) {
    if (!isWebView()) {
      return true
    }
    if (galleryStore.lightbox.isOpen && !galleryStore.lightbox.chromeVisible) {
      return true
    }
  }
  return false
})
const hasBackgroundImage = computed(() =>
  Boolean(resolveBackgroundImageUrl(settingsStore.appSettings.ui.background))
)
</script>

<template>
  <div class="relative h-screen w-screen bg-transparent">
    <WindowResizeOverlay />

    <div class="pointer-events-none absolute inset-0 z-0 overflow-hidden">
      <div
        class="app-background-image absolute inset-0 h-full w-full"
        :class="[isHome && 'app-background-image-no-blur']"
      />
      <div v-if="!isHome || !hasBackgroundImage" class="app-background-overlay absolute inset-0" />
    </div>

    <div
      class="relative z-10 flex h-full w-full min-w-0 flex-col rounded-lg text-foreground"
      :class="[!isHome && !isWelcome && !isGallery && 'surface-middle']"
    >
      <!-- 窄屏触摸暗房进入纯图片状态时，整个应用 Header 一并移除，内容区填满窗口。 -->
      <AppHeader v-if="!shouldHideAppHeader" />
      <!-- 主内容区域 -->
      <div class="relative z-10 min-h-0 flex-1 overflow-auto">
        <ContentArea />
      </div>
    </div>
    <GalleryDebugOverlay v-if="isDev" />
  </div>

  <!-- Toast 通知 -->
  <Toaster position="bottom-right" />
</template>
