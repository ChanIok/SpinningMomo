<script setup lang="ts">
import { NLayout } from 'naive-ui'
import AppSidebar from '@/layouts/AppSidebar.vue'
import AppContent from '@/layouts/AppContent.vue'
</script>

<template>
  <n-layout has-sider class="min-h-screen">
    <app-sidebar />
    <app-content />
  </n-layout>
</template>

<style scoped>
/* 扁平化设计，无需额外样式 */
</style>