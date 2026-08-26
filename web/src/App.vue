<script setup lang="ts">
import AppLayout from '@/components/layout/AppLayout.vue'
import { useViewportInsets } from '@/composables/useViewportInsets'

useViewportInsets()
</script>

<template>
  <AppLayout />
</template>

<style scoped></style>
