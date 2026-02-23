<script setup lang="ts">
import { useRoute } from 'vue-router'
import { isWebView } from '@/core/env'
import { SidebarProvider } from '@/components/ui/sidebar'
import { Toaster } from '@/components/ui/sonner'
import ActivityBar from './ActivityBar.vue'
import AppHeader from './AppHeader.vue'
import ContentArea from './ContentArea.vue'
import 'vue-sonner/style.css'

const route = useRoute()
const showHeader = isWebView()
const isHome = () => route.name === 'home'
</script>

<template>
  <SidebarProvider>
    <div class="relative h-screen w-screen overflow-hidden bg-transparent">
      <div class="pointer-events-none absolute inset-0 z-0">
        <div class="app-background-image absolute inset-0" />
        <div v-if="isHome()" class="app-background-overlay absolute inset-y-0 left-0 w-14" />
        <div v-else class="app-background-overlay absolute inset-0" />
      </div>

      <div class="relative z-10 flex h-full w-full flex-row">
        <!-- 左侧 ActivityBar -->
        <ActivityBar />

        <!-- 右侧：Header + 主内容区 -->
        <div
          class="relative flex min-h-0 flex-1 flex-col overflow-hidden text-foreground"
          :class="[
            !isHome() && 'surface-middle',
            showHeader ? 'rounded-lg' : 'rounded-tr-lg rounded-br-lg rounded-bl-lg',
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
