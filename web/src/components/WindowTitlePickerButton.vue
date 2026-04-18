<script setup lang="ts">
import { computed, ref } from 'vue'
import { Monitor } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { ScrollArea } from '@/components/ui/scroll-area'
import { call } from '@/core/rpc'
import { cn } from '@/lib/utils'
import { useI18n } from '@/composables/useI18n'

interface VisibleWindowTitleResult {
  title: string
}

const props = defineProps<{
  disabled?: boolean
  buttonClass?: string
}>()

const emit = defineEmits<{
  (e: 'select', title: string): void
}>()

const { t } = useI18n()

const isOpen = ref(false)
const isLoading = ref(false)
const loadFailed = ref(false)
const visibleWindows = ref<VisibleWindowTitleResult[]>([])

const buttonClasses = computed(() => cn('shrink-0', props.buttonClass))

const shouldQuoteWindowTitle = (title: string) => title.trim() !== title

const formatWindowTitle = (title: string) => (shouldQuoteWindowTitle(title) ? `"${title}"` : title)

const loadVisibleWindows = async () => {
  isLoading.value = true
  loadFailed.value = false

  try {
    visibleWindows.value = await call<VisibleWindowTitleResult[]>(
      'windowControl.listVisibleWindows',
      {}
    )
  } catch (error) {
    visibleWindows.value = []
    loadFailed.value = true
    console.error('Failed to list visible windows:', error)
  } finally {
    isLoading.value = false
  }
}

const handleOpenChange = (nextOpen: boolean) => {
  isOpen.value = nextOpen
  if (nextOpen) {
    void loadVisibleWindows()
  }
}

const handleSelect = (title: string) => {
  emit('select', title)
  isOpen.value = false
}
</script>

<template>
  <Popover :open="isOpen" @update:open="handleOpenChange">
    <PopoverTrigger as-child>
      <Button
        type="button"
        variant="outline"
        size="icon-sm"
        :disabled="disabled"
        :class="buttonClasses"
        :title="t('common.windowTitlePicker.trigger')"
      >
        <Monitor class="size-4" />
      </Button>
    </PopoverTrigger>

    <PopoverContent align="end" class="w-80 max-w-[calc(100vw-2rem)] p-1 pr-0">
      <div class="flex flex-col">
        <div v-if="isLoading" class="px-2 py-2 text-sm text-muted-foreground">
          {{ t('common.windowTitlePicker.loading') }}
        </div>

        <div v-else-if="loadFailed" class="flex items-center justify-between gap-2 px-2 py-2">
          <span class="text-sm text-muted-foreground">
            {{ t('common.windowTitlePicker.loadFailed') }}
          </span>
          <Button type="button" variant="ghost" size="sm" @click="void loadVisibleWindows()">
            {{ t('common.windowTitlePicker.retry') }}
          </Button>
        </div>

        <div
          v-else-if="visibleWindows.length === 0"
          class="px-2 py-2 text-sm text-muted-foreground"
        >
          {{ t('common.windowTitlePicker.empty') }}
        </div>

        <ScrollArea v-else class="max-h-64">
          <div>
            <button
              v-for="window in visibleWindows"
              :key="window.title"
              type="button"
              class="flex w-full rounded-md px-2 py-2 text-left text-sm transition-colors hover:bg-accent hover:text-accent-foreground"
              @click="handleSelect(window.title)"
            >
              <span class="break-all whitespace-pre-wrap text-foreground">
                {{ formatWindowTitle(window.title) }}
              </span>
            </button>
          </div>
        </ScrollArea>
      </div>
    </PopoverContent>
  </Popover>
</template>
