<script setup lang="ts">
import { isWebView } from '@/core/env'
import { SidebarProvider } from '@/components/ui/sidebar'
import ActivityBar from './ActivityBar.vue'
import AppHeader from './AppHeader.vue'
import ContentArea from './ContentArea.vue'

const showHeader = isWebView()
</script>

<template>
  <SidebarProvider>
    <div class="relative flex h-screen w-screen flex-col text-foreground">
      <!-- 窗口控制栏（仅 WebView 环境下显示） -->
      <AppHeader v-if="showHeader" />

      <!-- 主布局区域 -->
      <div class="flex flex-1 overflow-hidden">
        <!-- 左侧导航栏 -->
        <ActivityBar />

        <!-- 主内容区域 -->
        <div class="flex flex-1 overflow-hidden">
          <ContentArea />
        </div>
      </div>
    </div>
  </SidebarProvider>
</template>
