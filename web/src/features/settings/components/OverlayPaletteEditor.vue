<script setup lang="ts">
import type { HTMLAttributes } from 'vue'
import { computed, ref, watch } from 'vue'
import { Check, Palette } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { ToggleGroup, ToggleGroupItem } from '@/components/ui/toggle-group'
import { cn } from '@/lib/utils'
import { useI18n } from '@/composables/useI18n'
import type { OverlayColorMode, OverlayPalette } from '../overlayPalette'
import {
  buildOverlayGradient,
  getActiveOverlayColors,
  normalizeHexColor,
  OVERLAY_PALETTE_PRESETS,
} from '../overlayPalette'

const props = defineProps<{
  modelValue: OverlayPalette
  class?: HTMLAttributes['class']
}>()

const emits = defineEmits<{
  (e: 'update:modelValue', value: OverlayPalette): void
}>()

const { t } = useI18n()

const localPalette = ref<OverlayPalette>({
  mode: props.modelValue.mode,
  colors: [...props.modelValue.colors] as OverlayPalette['colors'],
})
const activeColorIndex = ref(0)
const activeHexInput = ref(localPalette.value.colors[0] ?? '#000000')

watch(
  () => props.modelValue,
  (value) => {
    localPalette.value = {
      mode: value.mode,
      colors: [...value.colors] as OverlayPalette['colors'],
    }
  },
  { deep: true }
)

watch(
  () => localPalette.value.mode,
  (mode) => {
    if (activeColorIndex.value >= mode) {
      activeColorIndex.value = mode - 1
    }
  }
)

const modeOptions: Array<{ value: OverlayColorMode; labelKey: string }> = [
  { value: 1, labelKey: 'settings.appearance.background.overlayPalette.mode.single' },
  { value: 2, labelKey: 'settings.appearance.background.overlayPalette.mode.double' },
  { value: 3, labelKey: 'settings.appearance.background.overlayPalette.mode.triple' },
  { value: 4, labelKey: 'settings.appearance.background.overlayPalette.mode.quad' },
]

const activeColors = computed(() => getActiveOverlayColors(localPalette.value))
const currentColor = computed(() => activeColors.value[activeColorIndex.value] ?? '#000000')
const modeLabel = computed(() => {
  const found = modeOptions.find((option) => option.value === localPalette.value.mode)
  return found ? t(found.labelKey) : ''
})

watch(
  currentColor,
  (value) => {
    activeHexInput.value = value
  },
  { immediate: true }
)

const triggerPreviewStyle = computed(() => ({
  backgroundImage: buildOverlayGradient(localPalette.value),
}))

const setPalette = (nextPalette: OverlayPalette) => {
  localPalette.value = {
    mode: nextPalette.mode,
    colors: [...nextPalette.colors] as OverlayPalette['colors'],
  }
  emits('update:modelValue', localPalette.value)
}

const updateMode = (mode: OverlayColorMode) => {
  setPalette({
    mode,
    colors: [...localPalette.value.colors] as OverlayPalette['colors'],
  })
}

const updateColor = (index: number, color: string) => {
  const normalizedColor = normalizeHexColor(color, currentColor.value)
  const nextColors = [...localPalette.value.colors] as OverlayPalette['colors']
  nextColors[index] = normalizedColor
  setPalette({
    mode: localPalette.value.mode,
    colors: nextColors,
  })
}

const handleModeChange = (value: unknown) => {
  if (value === null || value === undefined || Array.isArray(value)) return
  if (typeof value !== 'string' && typeof value !== 'number' && typeof value !== 'bigint') return

  const mode = Number(value) as OverlayColorMode
  if (mode < 1 || mode > 4) return
  updateMode(mode)
}

const handleHexCommit = () => {
  const normalized = normalizeHexColor(activeHexInput.value, currentColor.value)
  activeHexInput.value = normalized
  updateColor(activeColorIndex.value, normalized)
}

const applyPreset = (preset: OverlayPalette) => {
  activeColorIndex.value = 0
  setPalette({
    mode: preset.mode,
    colors: [...preset.colors] as OverlayPalette['colors'],
  })
}

const isPresetActive = (preset: OverlayPalette): boolean => {
  if (preset.mode !== localPalette.value.mode) return false
  return preset.colors.every((color, index) => color === localPalette.value.colors[index])
}

const getPreviewStyle = (palette: OverlayPalette) => ({
  backgroundImage: buildOverlayGradient(palette),
})
</script>

<template>
  <Popover>
    <PopoverTrigger as-child>
      <Button
        variant="outline"
        size="sm"
        :class="
          cn(
            'h-9 w-[13rem] justify-start gap-2 px-2',
            'border-border/80 bg-transparent hover:bg-accent',
            props.class
          )
        "
      >
        <div
          class="h-5 w-14 shrink-0 rounded-sm border border-border/70"
          :style="triggerPreviewStyle"
        />
        <span class="text-xs text-muted-foreground">{{ modeLabel }}</span>
        <Palette class="ml-auto h-4 w-4 text-muted-foreground" />
      </Button>
    </PopoverTrigger>

    <PopoverContent align="end" class="w-[21rem] space-y-4">
      <div class="space-y-2">
        <p class="text-xs font-medium text-foreground">
          {{ t('settings.appearance.background.overlayPalette.mode.title') }}
        </p>
        <ToggleGroup
          type="single"
          variant="outline"
          size="sm"
          :model-value="String(localPalette.mode)"
          @update:model-value="(value) => handleModeChange(value)"
        >
          <ToggleGroupItem
            v-for="option in modeOptions"
            :key="option.value"
            :value="String(option.value)"
            class="px-2 text-xs"
          >
            {{ t(option.labelKey) }}
          </ToggleGroupItem>
        </ToggleGroup>
      </div>

      <div class="space-y-2">
        <div
          class="h-16 rounded-md border border-border/70"
          :style="{ backgroundImage: buildOverlayGradient(localPalette) }"
        />

        <div class="flex items-center gap-2">
          <button
            v-for="(color, index) in activeColors"
            :key="`${index}-${color}`"
            type="button"
            :class="
              cn(
                'relative h-7 w-7 rounded-full border border-border/80 transition-all',
                activeColorIndex === index && 'ring-2 ring-primary/70 ring-offset-1'
              )
            "
            :style="{ backgroundColor: color }"
            @click="activeColorIndex = index"
          />

          <label
            class="relative h-9 w-11 shrink-0 cursor-pointer overflow-hidden rounded-md border border-input"
          >
            <div class="h-full w-full" :style="{ backgroundColor: currentColor }" />
            <input
              class="absolute inset-0 cursor-pointer opacity-0"
              type="color"
              :value="currentColor"
              @input="
                (event) => updateColor(activeColorIndex, (event.target as HTMLInputElement).value)
              "
            />
          </label>

          <Input
            v-model="activeHexInput"
            class="font-mono uppercase"
            placeholder="#000000"
            @keydown.enter.prevent="handleHexCommit"
            @blur="handleHexCommit"
          />
        </div>
      </div>

      <div class="space-y-2">
        <p class="text-xs font-medium text-foreground">
          {{ t('settings.appearance.background.overlayPalette.presets.title') }}
        </p>
        <div class="grid grid-cols-3 gap-2">
          <button
            v-for="preset in OVERLAY_PALETTE_PRESETS"
            :key="preset.id"
            type="button"
            :class="
              cn(
                'group relative h-8 overflow-hidden rounded-md border border-border/70 transition-colors',
                'hover:border-primary/50',
                isPresetActive(preset) && 'border-primary'
              )
            "
            :style="getPreviewStyle(preset)"
            @click="applyPreset(preset)"
          >
            <span
              class="absolute top-1 left-1 rounded bg-black/35 px-1 py-0.5 text-[10px] leading-none text-white"
            >
              {{ preset.mode }}C
            </span>
            <Check
              v-if="isPresetActive(preset)"
              class="absolute right-1 bottom-1 h-3.5 w-3.5 text-white drop-shadow-[0_1px_1px_rgba(0,0,0,0.45)]"
            />
          </button>
        </div>
      </div>
    </PopoverContent>
  </Popover>
</template>
