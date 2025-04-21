<script setup lang="ts">
import AppSidebar from '@/layouts/AppSidebar.vue'
import AppContent from '@/layouts/AppContent.vue'
import AppHeader from '@/layouts/AppHeader.vue'
import AppFooter from '@/layouts/AppFooter.vue'
</script>

<template>
  <div id="main-layout" class="flex flex-col h-screen bg-gray-100 dark:bg-gray-900">
    <!-- Header -->
    <app-header />

    <!-- Main Content -->
    <main class="flex flex-1 overflow-hidden">
      <app-sidebar />
      <app-content />
    </main>

    <!-- Footer -->
    <app-footer />
  </div>
</template>