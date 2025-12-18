<script setup lang="ts">
import type { HTMLAttributes } from "vue"
import { cn } from "@/lib/utils"

const props = defineProps<{
  class?: HTMLAttributes["class"]
}>()
</script>

<template>
  <div
    role="list"
    data-slot="item-group"
    :class="cn(
      'group/item-group flex flex-col',
      '[&>[data-slot=item]:not(:first-child)]:border-t-0',
      '[&>[data-slot=item]:not(:first-child):not(:last-child)]:rounded-none',
      '[&>[data-slot=item]:first-child]:rounded-b-none',
      '[&>[data-slot=item]:last-child]:rounded-t-none',
      props.class
    )"
  >
    <slot />
  </div>
</template>
