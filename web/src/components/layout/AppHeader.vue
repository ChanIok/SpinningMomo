<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRoute } from 'vue-router'
import { useGalleryLayout } from '@/features/gallery/composables'
import GalleryScanDialog from '@/features/gallery/components/GalleryScanDialog.vue'
import { useSettingsStore } from '@/features/settings/store'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { createInfinityNikkiAlbumScanParams } from '@/plugins/infinity_nikki'
import { Button } from '@/components/ui/button'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import {
  ChevronDown,
  Images,
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
const { toast } = useToast()
const settingsStore = useSettingsStore()
const showWindowControls = isWebView()
const isGalleryPage = computed(() => route.name === 'gallery')
const showInfinityNikkiPluginMenu = computed(() => {
  return isGalleryPage.value && settingsStore.appSettings.plugins.infinityNikki.enable
})
const infinityNikkiGameDir = computed(() =>
  settingsStore.appSettings.plugins.infinityNikki.gameDir.trim()
)
const infinityNikkiScanPreset = computed(() => {
  if (!infinityNikkiGameDir.value) {
    return undefined
  }
  return createInfinityNikkiAlbumScanParams(infinityNikkiGameDir.value)
})
const showInfinityNikkiImportDialog = ref(false)

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

const handleOpenInfinityNikkiImportDialog = () => {
  const gameDir = infinityNikkiGameDir.value
  if (!gameDir) {
    toast.error(t('app.header.gallery.plugin.infinityNikki.gameDirMissingTitle'), {
      description: t('app.header.gallery.plugin.infinityNikki.gameDirMissingDescription'),
    })
    return
  }

  showInfinityNikkiImportDialog.value = true
}
</script>

<template>
  <header class="flex h-10 items-center justify-between gap-2 bg-transparent pr-1 pl-4">
    <div v-if="showInfinityNikkiPluginMenu" class="flex items-center">
      <DropdownMenu>
        <DropdownMenuTrigger as-child>
          <Button variant="ghost" size="sm" class="h-8 hover:bg-black/10 dark:hover:bg-white/10">
            <span class="px-1 text-xs">
              {{ t('app.header.gallery.infinityNikki.menuTitle') }}
            </span>
            <ChevronDown class="h-3.5 w-3.5 opacity-70" />
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="start" class="w-52">
          <DropdownMenuItem @click="handleOpenInfinityNikkiImportDialog">
            <Images class="mr-2 h-4 w-4" />
            <span>{{ t('app.header.gallery.plugin.infinityNikki.importAlbum') }}</span>
          </DropdownMenuItem>
        </DropdownMenuContent>
      </DropdownMenu>
    </div>

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

  <GalleryScanDialog
    v-model:open="showInfinityNikkiImportDialog"
    :preset="infinityNikkiScanPreset"
  />
</template>

<style scoped>
.drag-region {
  -webkit-app-region: drag;
  app-region: drag;
}
</style>
