<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { useGalleryLayout } from '@/features/gallery/composables'
import { useI18n } from '@/composables/useI18n'
import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { Button } from '@/components/ui/button'
import {
  Minus,
  PanelLeftClose,
  PanelLeftOpen,
  PanelRightClose,
  PanelRightOpen,
  Square,
  X,
} from 'lucide-vue-next'

const route = useRoute()
const { t } = useI18n()
const showWindowControls = isWebView()
const isGalleryPage = computed(() => route.name === 'gallery')

const { isSidebarOpen, isDetailsOpen, toggleSidebar, toggleDetails } = useGalleryLayout()

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

const handleToggleSidebar = () => {
  toggleSidebar()
}

const handleToggleDetails = () => {
  toggleDetails()
}
</script>

<template>
  <header class="flex h-10 items-center justify-between gap-2 bg-transparent pr-1 pl-4">
    <!-- 可拖动区域 -->
    <div class="drag-region h-full flex-1" />

    <!-- 图库布局控制 -->
    <div v-if="isGalleryPage" class="flex gap-1">
      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 hover:bg-black/10 dark:hover:bg-white/10"
        :class="[!isSidebarOpen && 'text-muted-foreground']"
        :title="
          isSidebarOpen
            ? t('app.header.gallery.toggleSidebar.hide')
            : t('app.header.gallery.toggleSidebar.show')
        "
        @click="handleToggleSidebar"
      >
        <component :is="isSidebarOpen ? PanelLeftClose : PanelLeftOpen" class="h-4 w-4" />
      </Button>

      <Button
        variant="ghost"
        size="icon"
        class="h-8 w-8 hover:bg-black/10 dark:hover:bg-white/10"
        :class="[!isDetailsOpen && 'text-muted-foreground']"
        :title="
          isDetailsOpen
            ? t('app.header.gallery.toggleDetails.hide')
            : t('app.header.gallery.toggleDetails.show')
        "
        @click="handleToggleDetails"
      >
        <component :is="isDetailsOpen ? PanelRightClose : PanelRightOpen" class="h-4 w-4" />
      </Button>
    </div>

    <!-- 窗口控制按钮 -->
    <div v-if="showWindowControls" class="flex gap-2">
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
