<script setup lang="ts">
import { NLayout, NLayoutContent } from 'naive-ui'
</script>

<template>
  <n-layout class="app-content">
    <n-layout-content class="content-container">
      <router-view />
    </n-layout-content>
  </n-layout>
</template>

<style scoped>
.app-content {
  height: 100vh;
}

.content-container {
  height: 100%;
  padding: 0;
  overflow-y: auto;
  overflow-x: hidden;
  display: flex;
  flex-direction: column;
  background-color: var(--n-color);
}
</style> 