<script setup lang="ts">
import type { Component } from 'vue'
import type { SidebarMenuButtonProps } from './SidebarMenuButtonChild.vue'
import { reactiveOmit } from '@vueuse/core'
import { Tooltip, TooltipContent, TooltipTrigger } from '@/components/ui/tooltip'
import SidebarMenuButtonChild from './SidebarMenuButtonChild.vue'
import { useSidebar } from './utils'

defineOptions({
  inheritAttrs: false,
})

const props = withDefaults(
  defineProps<
    SidebarMenuButtonProps & {
      tooltip?: string | Component
      showTooltipWhenExpanded?: boolean
      tooltipVariant?: 'default' | 'sidebar'
    }
  >(),
  {
    as: 'button',
    variant: 'default',
    size: 'default',
    showTooltipWhenExpanded: false,
    tooltipVariant: 'default',
  }
)

const { isMobile, state } = useSidebar()

const delegatedProps = reactiveOmit(props, 'tooltip', 'showTooltipWhenExpanded', 'tooltipVariant')
</script>

<template>
  <SidebarMenuButtonChild v-if="!tooltip" v-bind="{ ...delegatedProps, ...$attrs }">
    <slot />
  </SidebarMenuButtonChild>

  <Tooltip v-else>
    <TooltipTrigger as-child>
      <SidebarMenuButtonChild v-bind="{ ...delegatedProps, ...$attrs }">
        <slot />
      </SidebarMenuButtonChild>
    </TooltipTrigger>
    <TooltipContent
      side="right"
      align="center"
      :variant="tooltipVariant"
      :hidden="
        (state !== 'collapsed' && !showTooltipWhenExpanded) ||
        (isMobile && !showTooltipWhenExpanded)
      "
    >
      <template v-if="typeof tooltip === 'string'">
        {{ tooltip }}
      </template>
      <component :is="tooltip" v-else />
    </TooltipContent>
  </Tooltip>
</template>
