<script setup lang="ts">
import { NLayout, NLayoutContent } from 'naive-ui'
</script>

<template>
  <n-layout class="h-screen">
    <n-layout-content
      class="h-full box-border pl-4 pt-4 overflow-y-auto overflow-x-hidden flex flex-col"
      :style="{ backgroundColor: 'var(--n-color)' }"
    >
      <router-view class="border-l border-t border-[#d8d8d8] rounded-tl-xl overflow-hidden" />
    </n-layout-content>
  </n-layout>
</template>
