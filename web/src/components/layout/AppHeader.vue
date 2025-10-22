<script setup lang="ts">
import { ref } from 'vue'
import { call } from '@/core/rpc'
import { Button } from '@/components/ui/button'
import { Minus, Square, X, Minimize2 } from 'lucide-vue-next'

const isMaximized = ref(false)

const handleMinimize = () => {
  call('webview.minimize').catch((err) => {
    console.error('Failed to minimize window:', err)
  })
}

const handleMaximizeToggle = () => {
  const method = isMaximized.value ? 'webview.restore' : 'webview.maximize'
  call(method)
    .then(() => {
      isMaximized.value = !isMaximized.value
    })
    .catch((err) => {
      console.error(`Failed to ${method} window:`, err)
    })
}

const handleClose = () => {
  call('webview.close').catch((err) => {
    console.error('Failed to close window:', err)
  })
}
</script>

<template>
  <header class="flex h-12 items-center justify-between gap-2 bg-transparent px-4">
    <!-- 可拖动区域 -->
    <div class="drag-region h-full flex-1" />

    <!-- 窗口控制按钮 -->
    <div class="flex gap-2">
      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 text-muted-foreground"
        @click="handleMinimize"
        title="Minimize"
      >
        <Minus class="h-4 w-4" />
      </Button>

      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 text-muted-foreground"
        @click="handleMaximizeToggle"
        :title="isMaximized ? 'Restore' : 'Maximize'"
      >
        <Minimize2 v-if="isMaximized" class="h-4 w-4" />
        <Square v-else class="h-4 w-4" />
      </Button>

      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 text-muted-foreground hover:bg-destructive hover:text-destructive-foreground"
        @click="handleClose"
        title="Close"
      >
        <X class="h-4 w-4" />
      </Button>
    </div>
  </header>
</template>

<style scoped>
.drag-region {
  -webkit-app-region: drag;
  app-region: drag;
}
</style>
