<script setup lang="ts">
import { computed, ref } from 'vue'
import { useEventListener, useMediaQuery } from '@vueuse/core'
import { useI18n } from '@/composables/useI18n'
import { heroAnimating } from '../../composables/useHeroTransition'

const props = defineProps<{
  canPrevious: boolean
  canNext: boolean
}>()

const emit = defineEmits<{
  previous: []
  next: []
}>()

const { t } = useI18n()
const touchPrimary = useMediaQuery('(hover: none) and (pointer: coarse)')
const lastInputModality = ref<'touch' | 'fine' | 'keyboard' | null>(null)

const showPointerNavigation = computed(() => {
  if (lastInputModality.value === 'touch') {
    return false
  }

  if (lastInputModality.value === 'fine' || lastInputModality.value === 'keyboard') {
    return true
  }

  return !touchPrimary.value
})

function handlePointerDown(event: PointerEvent) {
  lastInputModality.value = event.pointerType === 'touch' ? 'touch' : 'fine'
}

function handleKeydown() {
  lastInputModality.value = 'keyboard'
}

useEventListener(window, 'pointerdown', handlePointerDown)
useEventListener(window, 'keydown', handleKeydown)
</script>

<template>
  <button
    v-if="showPointerNavigation && props.canPrevious"
    type="button"
    class="surface-top absolute top-1/2 left-4 z-20 inline-flex h-12 w-12 -translate-y-1/2 items-center justify-center rounded-full text-foreground/75 opacity-60 transition-all duration-200 hover:scale-105 hover:bg-black/50 hover:text-foreground hover:opacity-100 hover:shadow-lg active:scale-95 dark:hover:bg-white/20"
    :style="heroAnimating ? { opacity: 0, pointerEvents: 'none' } : {}"
    :aria-label="t('gallery.lightbox.image.previousTitle')"
    @click="emit('previous')"
  >
    <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
      <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 19l-7-7 7-7" />
    </svg>
  </button>

  <button
    v-if="showPointerNavigation && props.canNext"
    type="button"
    class="surface-top absolute top-1/2 right-4 z-20 inline-flex h-12 w-12 -translate-y-1/2 items-center justify-center rounded-full text-foreground/75 opacity-60 transition-all duration-200 hover:scale-105 hover:bg-black/50 hover:text-foreground hover:opacity-100 hover:shadow-lg active:scale-95 dark:hover:bg-white/20"
    :style="heroAnimating ? { opacity: 0, pointerEvents: 'none' } : {}"
    :aria-label="t('gallery.lightbox.image.nextTitle')"
    @click="emit('next')"
  >
    <svg class="h-6 w-6" fill="none" stroke="currentColor" viewBox="0 0 24 24">
      <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 5l7 7-7 7" />
    </svg>
  </button>
</template>
