<script setup lang="ts">
import type { HTMLAttributes } from 'vue'
import { cn } from '@/lib/utils'

const props = defineProps<{
  class?: HTMLAttributes['class']
}>()
</script>

<template>
  <span
    data-slot="dropdown-menu-shortcut"
    :class="
      cn('ml-auto text-xs tracking-widest text-muted-foreground [&_kbd]:font-normal', props.class)
    "
  >
    <slot />
  </span>
</template>
