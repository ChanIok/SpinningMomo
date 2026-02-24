<script setup lang="ts">
import { ref, computed, watch } from 'vue'
import { Input } from '@/components/ui/input'
import { hexToHsv, hsvToHex, normalizeToHex } from './colorUtils'

const props = withDefaults(
  defineProps<{
    modelValue: string
    showHexInput?: boolean
  }>(),
  {
    showHexInput: true,
  }
)

const emit = defineEmits<{
  (e: 'update:modelValue', value: string): void
}>()

const hsv = ref(hexToHsv(props.modelValue))

// Keep HSV in sync with external modelValue changes
watch(
  () => props.modelValue,
  (newHex) => {
    const currentHex = hsvToHex(hsv.value)
    if (newHex !== currentHex) {
      hsv.value = hexToHsv(normalizeToHex(newHex))
    }
  }
)

const emitColor = () => {
  emit('update:modelValue', hsvToHex(hsv.value))
}

const handleHexInput = (e: Event) => {
  const target = e.target as HTMLInputElement
  const newHex = target.value
  if (/^#?[0-9A-Fa-f]{6}$/.test(newHex)) {
    const normalizedHex = newHex.startsWith('#') ? newHex.toUpperCase() : '#' + newHex.toUpperCase()
    hsv.value = hexToHsv(normalizedHex)
    emitColor()
  }
}

// --- Saturation & Brightness Board ---
const boardRef = ref<HTMLElement | null>(null)
const isDraggingBoard = ref(false)

const handleBoardPointerDown = (e: PointerEvent) => {
  if (!boardRef.value) return
  isDraggingBoard.value = true
  boardRef.value.setPointerCapture(e.pointerId)
  updateBoardPosition(e)
}

const handleBoardPointerMove = (e: PointerEvent) => {
  if (!isDraggingBoard.value) return
  updateBoardPosition(e)
}

const handleBoardPointerUp = (e: PointerEvent) => {
  if (!boardRef.value) return
  isDraggingBoard.value = false
  boardRef.value.releasePointerCapture(e.pointerId)
}

const updateBoardPosition = (e: PointerEvent) => {
  if (!boardRef.value) return
  const rect = boardRef.value.getBoundingClientRect()

  let x = e.clientX - rect.left
  let y = e.clientY - rect.top

  x = Math.max(0, Math.min(x, rect.width))
  y = Math.max(0, Math.min(y, rect.height))

  hsv.value.s = (x / rect.width) * 100
  hsv.value.v = 100 - (y / rect.height) * 100

  emitColor()
}

// --- Hue Slider ---
const hueRef = ref<HTMLElement | null>(null)
const isDraggingHue = ref(false)

const handleHuePointerDown = (e: PointerEvent) => {
  if (!hueRef.value) return
  isDraggingHue.value = true
  hueRef.value.setPointerCapture(e.pointerId)
  updateHuePosition(e)
}

const handleHuePointerMove = (e: PointerEvent) => {
  if (!isDraggingHue.value) return
  updateHuePosition(e)
}

const handleHuePointerUp = (e: PointerEvent) => {
  if (!hueRef.value) return
  isDraggingHue.value = false
  hueRef.value.releasePointerCapture(e.pointerId)
}

const updateHuePosition = (e: PointerEvent) => {
  if (!hueRef.value) return
  const rect = hueRef.value.getBoundingClientRect()

  let x = e.clientX - rect.left
  x = Math.max(0, Math.min(x, rect.width))

  hsv.value.h = (x / rect.width) * 360
  emitColor()
}

// Compute Styles
const boardBackground = computed(() => {
  return `hsl(${hsv.value.h}, 100%, 50%)`
})

const thumbPosition = computed(() => {
  return {
    left: `${hsv.value.s}%`,
    top: `${100 - hsv.value.v}%`,
  }
})

const hueThumbPosition = computed(() => {
  return {
    left: `${(hsv.value.h / 360) * 100}%`,
  }
})

const currentColorRender = computed(() => hsvToHex(hsv.value))
</script>

<template>
  <div class="flex w-[200px] flex-col gap-3">
    <!-- Saturation/Brightness Board -->
    <div
      ref="boardRef"
      class="relative h-[150px] w-full cursor-crosshair overflow-hidden rounded-md border border-border/80"
      :style="{ backgroundColor: boardBackground }"
      @pointerdown="handleBoardPointerDown"
      @pointermove="handleBoardPointerMove"
      @pointerup="handleBoardPointerUp"
    >
      <div
        class="pointer-events-none absolute inset-0 bg-gradient-to-r from-white to-transparent"
      />
      <div
        class="pointer-events-none absolute inset-0 bg-gradient-to-t from-black to-transparent"
      />

      <!-- Board Thumb -->
      <div
        class="pointer-events-none absolute h-3 w-3 -translate-x-1.5 -translate-y-1.5 rounded-full border border-white shadow-[0_0_2px_rgba(0,0,0,0.6)]"
        :style="{
          left: thumbPosition.left,
          top: thumbPosition.top,
          backgroundColor: currentColorRender,
        }"
      />
    </div>

    <!-- Hue Slider -->
    <div
      ref="hueRef"
      class="h-slider relative h-[12px] w-full cursor-pointer rounded-full border border-border/80"
      @pointerdown="handleHuePointerDown"
      @pointermove="handleHuePointerMove"
      @pointerup="handleHuePointerUp"
    >
      <!-- Hue Thumb -->
      <div
        class="pointer-events-none absolute top-1/2 h-3 w-3 -translate-x-1.5 -translate-y-1/2 rounded-full border border-white bg-white shadow-[0_0_2px_rgba(0,0,0,0.6)]"
        :style="{ left: hueThumbPosition.left }"
      />
    </div>

    <!-- Hex Input & Preview -->
    <div v-if="props.showHexInput" class="mt-1 flex items-center gap-2">
      <div
        class="h-6 w-6 shrink-0 rounded-md border border-border/80"
        :style="{ backgroundColor: currentColorRender }"
      />
      <Input
        :model-value="currentColorRender"
        @input="handleHexInput"
        class="h-7 flex-1 font-mono text-xs uppercase"
        placeholder="#000000"
      />
    </div>
  </div>
</template>

<style scoped>
.h-slider {
  background: linear-gradient(
    to right,
    #ff0000 0%,
    #ffff00 17%,
    #00ff00 33%,
    #00ffff 50%,
    #0000ff 67%,
    #ff00ff 83%,
    #ff0000 100%
  );
}
</style>
