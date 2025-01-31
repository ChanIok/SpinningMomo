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
.content-container {
  height: 100%;
  padding: 0;
  display: flex;
  flex-direction: column;
  background-color: var(--n-color);
}
</style> 