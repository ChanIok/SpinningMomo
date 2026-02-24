<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { isWebView } from '@/core/env'
import { SidebarProvider } from '@/components/ui/sidebar'
import { Toaster } from '@/components/ui/sonner'
import ActivityBar from './ActivityBar.vue'
import AppHeader from './AppHeader.vue'
import ContentArea from './ContentArea.vue'
import 'vue-sonner/style.css'

const route = useRoute()
const isWelcome = computed(() => route.name === 'welcome')
const showHeader = computed(() => isWebView())
const isHome = computed(() => route.name === 'home')
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
          :class="[isHome && 'app-background-overlay-home-clip']"
        />
      </div>

      <div class="relative z-10 flex h-full w-full flex-row">
        <!-- 左侧 ActivityBar -->
        <ActivityBar v-if="!isWelcome" :is-home="isHome" />

        <!-- 右侧：Header + 主内容区 -->
        <div
          class="relative flex min-h-0 flex-1 flex-col overflow-hidden text-foreground"
          :class="[
            !isHome && !isWelcome && 'surface-middle',
            isWelcome
              ? 'rounded-lg'
              : showHeader
                ? 'rounded-lg'
                : 'rounded-tr-lg rounded-br-lg rounded-bl-lg',
          ]"
        >
          <!-- 窗口控制栏（仅 WebView 环境下显示，位于主内容区上方） -->
          <AppHeader v-if="showHeader" />
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
