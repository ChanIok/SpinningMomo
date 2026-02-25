<script setup lang="ts">
import { call } from '@/core/rpc'
import { Button } from '@/components/ui/button'
import { Minus, Square, X } from 'lucide-vue-next'

const handleMinimize = () => {
  call('webview.minimize').catch((err) => {
    console.error('Failed to minimize window:', err)
  })
}

const handleMaximizeToggle = () => {
  call('webview.toggleMaximize').catch((err) => {
    console.error('Failed to toggle maximize window:', err)
  })
}

const handleClose = () => {
  call('webview.close').catch((err) => {
    console.error('Failed to close window:', err)
  })
}
</script>

<template>
  <header class="flex h-10 items-center justify-between gap-2 bg-transparent pr-1 pl-4">
    <!-- 可拖动区域 -->
    <div class="drag-region h-full flex-1" />

    <!-- 窗口控制按钮 -->
    <div class="flex gap-2">
      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 text-foreground hover:bg-black/10 dark:hover:bg-white/10"
        @click="handleMinimize"
        title="Minimize"
      >
        <Minus class="h-4 w-4" />
      </Button>

      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 text-foreground hover:bg-black/10 dark:hover:bg-white/10"
        @click="handleMaximizeToggle"
        title="Maximize / Restore"
      >
        <Square class="h-4 w-4" />
      </Button>

      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 text-foreground hover:bg-destructive hover:text-destructive-foreground"
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
