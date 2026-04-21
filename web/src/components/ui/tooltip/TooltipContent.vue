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
          'z-50 w-fit animate-in rounded-md px-3 py-1.5 text-xs text-balance fade-in-0 zoom-in-95 data-[side=bottom]:slide-in-from-top-2 data-[side=left]:slide-in-from-right-2 data-[side=right]:slide-in-from-left-2 data-[side=top]:slide-in-from-bottom-2 data-[state=closed]:animate-out data-[state=closed]:fade-out-0 data-[state=closed]:zoom-out-95',
          props.variant === 'sidebar'
            ? 'bg-background text-foreground shadow-sm'
            : 'bg-primary text-primary-foreground',
          props.class
        )
      "
    >
      <slot />

      <TooltipArrow
        class="z-50"
        :class="props.variant === 'sidebar' ? 'fill-background' : 'fill-primary'"
      />
    </TooltipContent>
  </TooltipPortal>
</template>
