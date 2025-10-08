<script setup lang="ts">
// 简单的路由容器，渲染当前路由组件
</script>

<template>
  <div class="flex h-full flex-1 flex-col">
    <main class="h-full flex-1 overflow-auto">
      <router-view />
    </main>
  </div>
</template>
