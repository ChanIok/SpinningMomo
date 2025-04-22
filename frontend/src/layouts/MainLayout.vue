<script setup lang="ts">
import AppHeader from '@/layouts/AppHeader.vue'
import AppFooter from '@/layouts/AppFooter.vue'
import AppActivityBar from '@/layouts/AppActivityBar.vue'
import AppSideBar from '@/layouts/AppSideBar.vue'
import AppContent from '@/layouts/AppContent.vue'
import AppAuxiliaryBar from '@/layouts/AppAuxiliaryBar.vue'
</script>

<template>
  <div id="main-layout" class="flex flex-col h-screen bg-gray-100 dark:bg-gray-900">
    <!-- Header -->
    <app-header />

    <!-- Main Content -->
    <main class="flex flex-1 overflow-hidden">
      <!-- 活动栏 -->
      <app-activity-bar />

      <!-- 侧边栏 -->
      <app-side-bar />

      <!-- 内容区 -->
      <app-content />

      <!-- 辅助栏 -->
      <app-auxiliary-bar />
    </main>

    <!-- Footer -->
    <app-footer />
  </div>
</template>