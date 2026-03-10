<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRoute } from 'vue-router'
import { useGalleryLayout } from '@/features/gallery/composables'
import GalleryScanDialog from '@/features/gallery/components/GalleryScanDialog.vue'
import { useSettingsStore } from '@/features/settings/store'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { call } from '@/core/rpc'
import { useTaskStore } from '@/core/tasks/store'
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
  CircleAlert,
  CircleCheck,
  ChevronDown,
  Images,
  ListTodo,
  Loader2,
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
const taskStore = useTaskStore()
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
const recentTasks = computed(() => taskStore.tasks.slice(0, 6))
const activeTaskCount = computed(() => taskStore.activeTasks.length)
const hasTaskRecords = computed(() => taskStore.tasks.length > 0)
const taskButtonText = computed(() => {
  if (activeTaskCount.value > 0) {
    return t('app.header.tasks.activeCount', { count: activeTaskCount.value })
  }
  return t('app.header.tasks.button')
})

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

function resolveTaskTypeLabel(type: string): string {
  if (type === 'gallery.scanDirectory') {
    return t('app.header.tasks.type.galleryScan')
  }
  if (type === 'plugins.infinityNikki.initialScan') {
    return t('app.header.tasks.type.infinityNikkiInitialScan')
  }
  if (type === 'plugins.infinityNikki.extractPhotoParams') {
    return t('app.header.tasks.type.infinityNikkiExtractPhotoParams')
  }
  if (type === 'plugins.infinityNikki.initializeScreenshotHardlinks') {
    return t('app.header.tasks.type.infinityNikkiInitScreenshotHardlinks')
  }
  return type
}

function resolveTaskStatusLabel(status: string): string {
  if (status === 'queued') {
    return t('app.header.tasks.status.queued')
  }
  if (status === 'running') {
    return t('app.header.tasks.status.running')
  }
  if (status === 'succeeded') {
    return t('app.header.tasks.status.succeeded')
  }
  if (status === 'failed') {
    return t('app.header.tasks.status.failed')
  }
  if (status === 'cancelled') {
    return t('app.header.tasks.status.cancelled')
  }
  return status
}

function resolveTaskPercent(task: {
  progress?: { percent?: number }
  status: string
}): number | null {
  const value = task.progress?.percent
  if (typeof value === 'number' && Number.isFinite(value)) {
    return Math.max(0, Math.min(100, Math.round(value)))
  }
  if (task.status === 'succeeded') {
    return 100
  }
  return null
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

    <div v-if="hasTaskRecords" class="flex items-center">
      <DropdownMenu>
        <DropdownMenuTrigger as-child>
          <Button variant="ghost" size="sm" class="h-8 hover:bg-black/10 dark:hover:bg-white/10">
            <Loader2 v-if="activeTaskCount > 0" class="mr-1.5 h-3.5 w-3.5 animate-spin" />
            <ListTodo v-else class="mr-1.5 h-3.5 w-3.5 opacity-70" />
            <span class="text-xs">{{ taskButtonText }}</span>
            <ChevronDown class="ml-1 h-3.5 w-3.5 opacity-70" />
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="end" class="w-96 p-2">
          <div v-if="recentTasks.length === 0" class="px-2 py-3 text-xs text-muted-foreground">
            {{ t('app.header.tasks.none') }}
          </div>
          <div v-else class="space-y-2">
            <div
              v-for="task in recentTasks"
              :key="task.taskId"
              class="rounded-md border border-border/60 p-2"
            >
              <div class="flex items-center justify-between gap-2">
                <span class="truncate text-xs font-medium">{{
                  resolveTaskTypeLabel(task.type)
                }}</span>
                <span
                  class="inline-flex items-center gap-1 text-[11px]"
                  :class="[
                    task.status === 'failed'
                      ? 'text-destructive'
                      : task.status === 'succeeded'
                        ? 'text-emerald-600'
                        : 'text-muted-foreground',
                  ]"
                >
                  <Loader2
                    v-if="task.status === 'queued' || task.status === 'running'"
                    class="h-3 w-3 animate-spin"
                  />
                  <CircleCheck v-else-if="task.status === 'succeeded'" class="h-3 w-3" />
                  <CircleAlert v-else class="h-3 w-3" />
                  {{ resolveTaskStatusLabel(task.status) }}
                </span>
              </div>

              <p v-if="task.context" class="mt-1 truncate text-[11px] text-muted-foreground">
                {{ task.context }}
              </p>

              <p
                v-if="task.progress?.message"
                class="mt-1 truncate text-[11px] text-muted-foreground"
              >
                {{ task.progress.message }}
              </p>

              <div v-if="resolveTaskPercent(task) !== null" class="mt-2 space-y-1">
                <div class="h-1.5 overflow-hidden rounded-full bg-muted">
                  <div
                    class="h-full bg-primary transition-all"
                    :style="{ width: `${resolveTaskPercent(task)}%` }"
                  />
                </div>
                <div class="text-right text-[10px] text-muted-foreground">
                  {{ resolveTaskPercent(task) }}%
                </div>
              </div>
            </div>
          </div>
        </DropdownMenuContent>
      </DropdownMenu>
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
