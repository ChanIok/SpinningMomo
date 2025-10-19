<script setup lang="ts">
import type { ScrollAreaRootProps } from "reka-ui"
import type { HTMLAttributes } from "vue"
import { ref, computed } from "vue"
import { reactiveOmit, unrefElement } from "@vueuse/core"
import {
  ScrollAreaCorner,
  ScrollAreaRoot,

  ScrollAreaViewport,
} from "reka-ui"
import { cn } from "@/lib/utils"
import ScrollBar from "./ScrollBar.vue"

const props = defineProps<ScrollAreaRootProps & { class?: HTMLAttributes["class"] }>()

const delegatedProps = reactiveOmit(props, "class")

// 暴露内部 viewport 的 ref，用于虚拟滚动等需要直接访问滚动容器的场景
const viewportComponentRef = ref<InstanceType<typeof ScrollAreaViewport> | null>(null)

// 通过 computed 获取真实的滚动容器元素
// unrefElement 返回的是 slot 内容的根元素，需要获取其父元素（真正的滚动容器）
const viewportElement = computed(() => unrefElement(viewportComponentRef)?.parentElement as HTMLElement | null)

defineExpose({
  viewportElement,
})
</script>

<template>
  <ScrollAreaRoot
    data-slot="scroll-area"
    v-bind="delegatedProps"
    :class="cn('relative', props.class)"
  >
    <ScrollAreaViewport
      ref="viewportComponentRef"
      data-slot="scroll-area-viewport"
      class="focus-visible:ring-ring/50 size-full rounded-[inherit] transition-[color,box-shadow] outline-none focus-visible:ring-[3px] focus-visible:outline-1"
    >
      <slot />
    </ScrollAreaViewport>
    <ScrollBar />
    <ScrollAreaCorner />
  </ScrollAreaRoot>
</template>
