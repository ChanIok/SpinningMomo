<script setup lang="ts">
import { computed, nextTick, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useGalleryLayout } from '@/features/gallery/composables'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { call } from '@/core/rpc'
import { useTaskStore } from '@/core/tasks/store'
import { isWebView } from '@/core/env'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'
import {
  CircleAlert,
  CircleCheck,
  ChevronDown,
  ChevronUp,
  ListTodo,
  Loader2,
  Minus,
  PanelLeftClose,
  PanelLeftOpen,
  PanelRightClose,
  PanelRightOpen,
  Square,
  Trash2,
  X,
} from 'lucide-vue-next'

const route = useRoute()
const { t } = useI18n()
const { toast } = useToast()
const taskStore = useTaskStore()
const showWindowControls = isWebView()
const isGalleryPage = computed(() => route.name === 'gallery')
const displayTasks = computed(() => taskStore.tasks)
const activeTaskCount = computed(() => taskStore.activeTasks.length)
const finishedTaskCount = computed(
  () =>
    taskStore.tasks.filter((item) => item.status !== 'queued' && item.status !== 'running').length
)
const hasFinishedTasks = computed(() => finishedTaskCount.value > 0)
const hasTaskRecords = computed(() => taskStore.tasks.length > 0)
const taskButtonText = computed(() => {
  if (activeTaskCount.value > 0) {
    return t('app.header.tasks.activeCount', { count: activeTaskCount.value })
  }
  return t('app.header.tasks.button')
})
const taskPanelSummaryText = computed(() => {
  if (activeTaskCount.value > 0) {
    return t('app.header.tasks.activeCount', { count: activeTaskCount.value })
  }

  return t('app.header.tasks.recordCount', { count: displayTasks.value.length })
})
const isTaskMenuOpen = ref(false)
const isClearingFinished = ref(false)
const expandedTaskIds = ref<Record<string, boolean>>({})
const overflowingTaskIds = ref<Record<string, boolean>>({})
const taskMessageElements = new Map<string, HTMLElement>()

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

function resolveTaskTypeLabel(type: string): string {
  if (type === 'gallery.scanDirectory') {
    return t('app.header.tasks.type.galleryScan')
  }
  if (type === 'extensions.infinityNikki.initialScan') {
    return t('app.header.tasks.type.infinityNikkiInitialScan')
  }
  if (type === 'extensions.infinityNikki.extractPhotoParams') {
    return t('app.header.tasks.type.infinityNikkiExtractPhotoParams')
  }
  if (type === 'extensions.infinityNikki.initializeScreenshotHardlinks') {
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

function resolveTaskMessage(task: {
  status: string
  errorMessage?: string
  progress?: { message?: string }
}): string | null {
  if (task.status === 'failed' && task.errorMessage) {
    return task.errorMessage
  }

  return task.progress?.message ?? null
}

function setTaskMessageRef(taskId: string, element: unknown): void {
  if (element instanceof HTMLElement) {
    taskMessageElements.set(taskId, element)
    return
  }

  taskMessageElements.delete(taskId)
}

function isTaskMessageExpanded(taskId: string): boolean {
  return expandedTaskIds.value[taskId] === true
}

function isTaskMessageExpandable(taskId: string): boolean {
  return overflowingTaskIds.value[taskId] === true
}

function toggleTaskMessageExpanded(taskId: string): void {
  expandedTaskIds.value = {
    ...expandedTaskIds.value,
    [taskId]: !isTaskMessageExpanded(taskId),
  }
}

function pruneTaskUiState(): void {
  const activeTaskIds = new Set(displayTasks.value.map((task) => task.taskId))

  expandedTaskIds.value = Object.fromEntries(
    Object.entries(expandedTaskIds.value).filter(([taskId]) => activeTaskIds.has(taskId))
  )
  overflowingTaskIds.value = Object.fromEntries(
    Object.entries(overflowingTaskIds.value).filter(([taskId]) => activeTaskIds.has(taskId))
  )

  for (const taskId of taskMessageElements.keys()) {
    if (!activeTaskIds.has(taskId)) {
      taskMessageElements.delete(taskId)
    }
  }
}

async function measureOverflowingTaskMessages(): Promise<void> {
  if (!isTaskMenuOpen.value) {
    return
  }

  await nextTick()

  const nextOverflowingTaskIds: Record<string, boolean> = {}
  for (const task of displayTasks.value) {
    const message = resolveTaskMessage(task)
    if (task.status !== 'failed' || !message) {
      continue
    }

    if (isTaskMessageExpanded(task.taskId)) {
      nextOverflowingTaskIds[task.taskId] = overflowingTaskIds.value[task.taskId] ?? false
      continue
    }

    const element = taskMessageElements.get(task.taskId)
    nextOverflowingTaskIds[task.taskId] =
      element?.scrollHeight !== undefined
        ? element.scrollHeight > element.clientHeight + 1
        : (overflowingTaskIds.value[task.taskId] ?? false)
  }

  overflowingTaskIds.value = nextOverflowingTaskIds
}

async function handleClearFinished(): Promise<void> {
  if (isClearingFinished.value || !hasFinishedTasks.value) {
    return
  }

  isClearingFinished.value = true
  try {
    await taskStore.clearFinished()
    pruneTaskUiState()
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('app.header.tasks.clearFailedTitle'), {
      description: message,
    })
  } finally {
    isClearingFinished.value = false
  }
}

watch(
  displayTasks,
  () => {
    pruneTaskUiState()
    if (isTaskMenuOpen.value) {
      void measureOverflowingTaskMessages()
    }
  },
  { deep: true }
)

watch(
  [isTaskMenuOpen, expandedTaskIds],
  ([isOpen]) => {
    if (!isOpen) {
      return
    }

    void measureOverflowingTaskMessages()
  },
  { deep: true }
)
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

    <div v-if="hasTaskRecords" class="flex items-center">
      <DropdownMenu v-model:open="isTaskMenuOpen">
        <DropdownMenuTrigger as-child>
          <Button variant="ghost" size="sm" class="h-8 hover:bg-black/10 dark:hover:bg-white/10">
            <Loader2 v-if="activeTaskCount > 0" class="mr-1.5 h-3.5 w-3.5 animate-spin" />
            <ListTodo v-else class="mr-1.5 h-3.5 w-3.5 opacity-70" />
            <span class="text-xs">{{ taskButtonText }}</span>
            <ChevronDown class="ml-1 h-3.5 w-3.5 opacity-70" />
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="end" class="w-[28rem] p-0">
          <div class="flex items-center justify-between gap-3 border-b border-border/60 px-3 py-2">
            <div class="min-w-0">
              <p class="text-sm font-medium text-foreground">{{ t('app.header.tasks.button') }}</p>
              <p class="text-[11px] text-muted-foreground">
                {{ taskPanelSummaryText }}
              </p>
            </div>
            <Button
              variant="ghost"
              size="sm"
              class="h-7 px-2 text-[11px]"
              :disabled="!hasFinishedTasks || isClearingFinished"
              @click="handleClearFinished"
            >
              <Loader2 v-if="isClearingFinished" class="mr-1.5 h-3 w-3 animate-spin" />
              <Trash2 v-else class="mr-1.5 h-3 w-3" />
              {{ t('app.header.tasks.clearFinished') }}
            </Button>
          </div>

          <div v-if="displayTasks.length === 0" class="px-3 py-4 text-xs text-muted-foreground">
            {{ t('app.header.tasks.none') }}
          </div>

          <ScrollArea v-else class="max-h-[28rem]">
            <div class="space-y-2 p-2">
              <div
                v-for="task in displayTasks"
                :key="task.taskId"
                class="rounded-md border border-border/60 p-2"
              >
                <div class="flex items-center justify-between gap-2">
                  <span class="truncate text-xs font-medium text-foreground">{{
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

                <p
                  v-if="task.context"
                  class="mt-1 text-[11px] leading-4 break-all text-muted-foreground"
                >
                  {{ task.context }}
                </p>

                <div v-if="resolveTaskMessage(task)" class="mt-1 flex items-start gap-2">
                  <p
                    :ref="(element) => setTaskMessageRef(task.taskId, element)"
                    class="min-w-0 flex-1 text-[11px] leading-4 break-words"
                    :class="[
                      task.status === 'failed' ? 'text-destructive' : 'text-muted-foreground',
                      isTaskMessageExpanded(task.taskId) ? 'line-clamp-none' : 'line-clamp-1',
                    ]"
                  >
                    {{ resolveTaskMessage(task) }}
                  </p>
                  <Button
                    v-if="task.status === 'failed' && isTaskMessageExpandable(task.taskId)"
                    variant="ghost"
                    size="sm"
                    class="h-5 px-1.5 text-[10px] text-muted-foreground"
                    @click.stop="toggleTaskMessageExpanded(task.taskId)"
                  >
                    {{
                      isTaskMessageExpanded(task.taskId)
                        ? t('app.header.tasks.collapse')
                        : t('app.header.tasks.expand')
                    }}
                    <component
                      :is="isTaskMessageExpanded(task.taskId) ? ChevronUp : ChevronDown"
                      class="ml-1 h-3 w-3"
                    />
                  </Button>
                </div>

                <div v-if="resolveTaskPercent(task) !== null" class="mt-2 space-y-1">
                  <div class="h-1.5 overflow-hidden rounded-full bg-muted">
                    <div
                      class="h-full transition-all"
                      :class="task.status === 'failed' ? 'bg-destructive' : 'bg-primary'"
                      :style="{ width: `${resolveTaskPercent(task)}%` }"
                    />
                  </div>
                  <div class="text-right text-[10px] text-muted-foreground">
                    {{ resolveTaskPercent(task) }}%
                  </div>
                </div>
              </div>
            </div>
          </ScrollArea>
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
</template>

<style scoped>
.drag-region {
  -webkit-app-region: drag;
  app-region: drag;
}
</style>
