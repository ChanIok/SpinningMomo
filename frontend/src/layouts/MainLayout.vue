<script setup lang="ts">
import AppSidebar from '@/layouts/AppSidebar.vue'
import AppContent from '@/layouts/AppContent.vue'
</script>

<template>
  <div id="main-layout" class="flex min-h-screen bg-gray-50">
    <app-sidebar />
    <app-content />
  </div>
</template>