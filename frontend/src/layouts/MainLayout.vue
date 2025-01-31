<script setup lang="ts">
import { NLayout } from 'naive-ui'
import AppSidebar from '@/layouts/AppSidebar.vue'
import AppContent from '@/layouts/AppContent.vue'
</script>

<template>
  <n-layout has-sider>
    <app-sidebar />
    <app-content />
  </n-layout>
</template>

<style scoped>

</style> 