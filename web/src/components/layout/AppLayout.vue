<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { useSettingsStore } from '@/features/settings/store'
import { resolveBackgroundImageUrl } from '@/features/settings/backgroundPath'
import { SidebarProvider } from '@/components/ui/sidebar'
import { Toaster } from '@/components/ui/sonner'
import ActivityBar from './ActivityBar.vue'
import AppHeader from './AppHeader.vue'
import ContentArea from './ContentArea.vue'
import 'vue-sonner/style.css'

const route = useRoute()
const settingsStore = useSettingsStore()
const isWelcome = computed(() => route.name === 'welcome')
const isHome = computed(() => route.name === 'home')
const hasBackgroundImage = computed(() =>
  Boolean(resolveBackgroundImageUrl(settingsStore.appSettings.ui.background))
)
</script>

<template>
  <SidebarProvider>
    <div class="relative h-screen w-screen overflow-hidden bg-transparent">
      <div class="pointer-events-none absolute inset-0 z-0">
        <div
          class="app-background-image absolute inset-0"
          :class="[isHome && 'app-background-image-no-blur']"
        />
        <div
          class="app-background-overlay absolute inset-0"
          :class="[isHome && hasBackgroundImage && 'app-background-overlay-home-clip']"
        />
      </div>

      <div class="relative z-10 flex h-full w-full flex-row">
        <!-- 左侧 ActivityBar -->
        <ActivityBar v-if="!isWelcome" :is-home="isHome" />

        <!-- 右侧：Header + 主内容区 -->
        <div
          class="relative flex min-h-0 flex-1 flex-col overflow-hidden rounded-lg text-foreground"
          :class="[!isHome && !isWelcome && 'surface-middle']"
        >
          <!-- 窗口控制栏 -->
          <AppHeader />
          <!-- 主内容区域 -->
          <div class="relative z-10 min-h-0 flex-1 overflow-auto">
            <ContentArea />
          </div>
        </div>
      </div>
    </div>

    <!-- Toast 通知 -->
    <Toaster position="bottom-right" richColors closeButton />
  </SidebarProvider>
</template>
