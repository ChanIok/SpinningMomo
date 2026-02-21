<script setup lang="ts">
import { isWebView } from '@/core/env'
import { SidebarProvider } from '@/components/ui/sidebar'
import { Toaster } from '@/components/ui/sonner'
import ActivityBar from './ActivityBar.vue'
import AppHeader from './AppHeader.vue'
import ContentArea from './ContentArea.vue'
import 'vue-sonner/style.css'

const showHeader = isWebView()
</script>

<template>
  <SidebarProvider>
    <div class="relative h-screen w-screen overflow-hidden bg-transparent">
      <div class="pointer-events-none absolute inset-0 z-0">
        <div class="app-background-image absolute inset-0" />
        <div class="app-background-overlay absolute inset-0" />
      </div>

      <div class="relative z-10 flex h-full w-full flex-col">
        <!-- 窗口控制栏（仅 WebView 环境下显示） -->
        <AppHeader v-if="showHeader" />
        <div class="flex min-h-0 flex-1" :class="{ 'pt-0': showHeader }">
          <div
            class="relative flex flex-1 flex-col overflow-hidden rounded-md bg-transparent text-foreground"
          >
            <!-- 主布局区域 -->
            <div class="relative z-10 flex min-h-0 flex-1">
              <!-- 左侧导航栏 -->
              <ActivityBar />

              <!-- 主内容区域 -->
              <div class="flex flex-1 overflow-auto">
                <ContentArea />
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Toast 通知 -->
    <Toaster position="bottom-right" richColors closeButton />
  </SidebarProvider>
</template>
