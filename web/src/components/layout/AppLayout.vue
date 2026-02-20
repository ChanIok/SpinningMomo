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
    <div class="flex h-screen w-screen flex-col bg-panel">
      <!-- 窗口控制栏（仅 WebView 环境下显示） -->
      <AppHeader v-if="showHeader" />
      <div class="flex min-h-0 flex-1 p-1" :class="{ 'pt-0': showHeader }">
        <div
          class="relative flex flex-1 flex-col overflow-hidden rounded-md bg-background text-foreground"
        >
          <div class="pointer-events-none absolute inset-0">
            <div
              class="absolute inset-0 transition-opacity duration-300"
              style="
                background-image: var(--app-background-image, none);
                background-size: cover;
                background-position: center;
                background-repeat: no-repeat;
                opacity: var(--app-background-opacity, 0);
                filter: blur(var(--app-background-blur, 0px));
                transform: scale(1.04);
              "
            />
          </div>

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

    <!-- Toast 通知 -->
    <Toaster position="bottom-right" richColors closeButton />
  </SidebarProvider>
</template>
