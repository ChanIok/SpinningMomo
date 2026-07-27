<script setup lang="ts">
import type { TooltipContentEmits, TooltipContentProps } from 'reka-ui'
import type { HTMLAttributes } from 'vue'
import { reactiveOmit } from '@vueuse/core'
import { TooltipArrow, TooltipContent, TooltipPortal, useForwardPropsEmits } from 'reka-ui'
import { cn } from '@/lib/utils'

defineOptions({
  inheritAttrs: false,
})

const props = withDefaults(
  defineProps<
    TooltipContentProps & {
      class?: HTMLAttributes['class']
      variant?: 'default' | 'sidebar'
    }
  >(),
  {
    sideOffset: 4,
    variant: 'default',
  }
)

const emits = defineEmits<TooltipContentEmits>()

const delegatedProps = reactiveOmit(props, 'class', 'variant')
const forwarded = useForwardPropsEmits(delegatedProps, emits)
</script>

<template>
  <TooltipPortal>
    <TooltipContent
      data-slot="tooltip-content"
      v-bind="{ ...forwarded, ...$attrs }"
      :class="
        cn(
          'z-50 w-fit animate-in rounded-md px-3 py-1.5 text-xs text-balance fade-in-0 select-none zoom-in-95 data-[side=bottom]:slide-in-from-top-2 data-[side=left]:slide-in-from-right-2 data-[side=right]:slide-in-from-left-2 data-[side=top]:slide-in-from-bottom-2 data-[state=closed]:animate-out data-[state=closed]:fade-out-0 data-[state=closed]:zoom-out-95',
          props.variant === 'sidebar'
            ? 'bg-background text-foreground dark:bg-foreground dark:text-background'
            : 'bg-foreground text-background',
          props.class
        )
      "
    >
      <slot />

      <TooltipArrow
        class="z-50 size-2.5 translate-y-[calc(-50%_-_2px)] rotate-45 rounded-xs"
        :class="
          props.variant === 'sidebar'
            ? 'bg-background fill-background dark:bg-foreground dark:fill-foreground'
            : 'bg-foreground fill-foreground'
        "
      />
    </TooltipContent>
  </TooltipPortal>
</template>
