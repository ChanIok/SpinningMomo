<script setup lang="ts">
import type { CSSProperties, HTMLAttributes } from 'vue'
import { computed, ref } from 'vue'
import { useResizeObserver, useVModel } from '@vueuse/core'
import { ChevronDown } from 'lucide-vue-next'
import { Input } from '@/components/ui/input'
import { Button } from '@/components/ui/button'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { ScrollArea } from '@/components/ui/scroll-area'
import { call } from '@/core/rpc'
import { cn } from '@/lib/utils'
import { useI18n } from '@/composables/useI18n'

interface VisibleWindowTitleResult {
  title: string
}

type PopoverWidth = 'trigger' | number | string

defineOptions({
  inheritAttrs: false,
})

const props = defineProps<{
  defaultValue?: string
  modelValue?: string
  disabled?: boolean
  class?: HTMLAttributes['class']
  inputClass?: HTMLAttributes['class']
  popoverWidth?: PopoverWidth
  balancedCenter?: boolean
  popoverNoShadow?: boolean
}>()

const emit = defineEmits<{
  (e: 'update:modelValue', payload: string): void
  (e: 'select', title: string): void
}>()

const { t } = useI18n()

const modelValue = useVModel(props, 'modelValue', emit, {
  passive: true,
  defaultValue: props.defaultValue,
})

const isOpen = ref(false)
const isLoading = ref(false)
const loadFailed = ref(false)
const visibleWindows = ref<VisibleWindowTitleResult[]>([])
const rootRef = ref<HTMLElement | null>(null)
const triggerWidthPx = ref<number | null>(null)

const rootClasses = computed(() => cn('relative w-full', props.class))
const inputClasses = computed(() =>
  cn(props.balancedCenter ? 'pl-9 pr-9' : 'pr-9', props.inputClass)
)
const resolvedPopoverWidth = computed(() => {
  if (props.popoverWidth == null || props.popoverWidth === 'trigger') {
    if (triggerWidthPx.value && triggerWidthPx.value > 0) {
      return `${triggerWidthPx.value}px`
    }
    return 'var(--reka-popover-trigger-width)'
  }

  if (typeof props.popoverWidth === 'number') {
    return `${props.popoverWidth}px`
  }

  return props.popoverWidth
})
const popoverContentStyle = computed<CSSProperties>(() => ({
  width: resolvedPopoverWidth.value,
}))

useResizeObserver(rootRef, (entries) => {
  const nextWidth = entries[0]?.contentRect.width
  if (typeof nextWidth === 'number' && nextWidth > 0) {
    triggerWidthPx.value = Math.round(nextWidth)
  }
})

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
  modelValue.value = title
  emit('select', title)
  isOpen.value = false
}
</script>

<template>
  <Popover :open="isOpen" @update:open="handleOpenChange">
    <div ref="rootRef" :class="rootClasses">
      <Input v-model="modelValue" :disabled="disabled" :class="inputClasses" v-bind="$attrs" />

      <PopoverTrigger as-child>
        <Button
          type="button"
          variant="ghost"
          size="icon-sm"
          :disabled="disabled"
          :title="t('common.windowTitlePicker.trigger')"
          class="absolute top-1/2 right-1 h-7 w-7 -translate-y-1/2 p-0 text-muted-foreground hover:text-foreground"
        >
          <ChevronDown class="size-4" />
        </Button>
      </PopoverTrigger>
    </div>

    <PopoverContent
      align="end"
      :side-offset="10"
      :align-offset="-4"
      :style="popoverContentStyle"
      :class="cn('max-w-[calc(100vw-2rem)] p-0', popoverNoShadow && '!shadow-none')"
    >
      <ScrollArea class="max-h-64 p-1">
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

          <div v-else>
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
        </div>
      </ScrollArea>
    </PopoverContent>
  </Popover>
</template>
