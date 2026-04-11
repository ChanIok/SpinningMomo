<script setup lang="ts">
import { nextTick, watch } from 'vue'
import { useGalleryContextMenu } from '../../composables/useGalleryContextMenu'
import GalleryAssetDropdownMenuContent from './GalleryAssetDropdownMenuContent.vue'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'

const contextMenu = useGalleryContextMenu()

watch(
  () => contextMenu.state.requestToken,
  async (token) => {
    if (token <= 0) {
      return
    }

    // 等待锚点位移先提交到 DOM，再以受控方式打开菜单，避免定位闪动。
    await nextTick()
    contextMenu.setOpen(true)
  }
)
</script>

<template>
  <div>
    <DropdownMenu
      :open="contextMenu.state.isOpen"
      :modal="false"
      @update:open="contextMenu.setOpen"
    >
      <DropdownMenuTrigger as-child>
        <div
          class="pointer-events-none fixed h-px w-px opacity-0"
          :style="{
            left: `${contextMenu.state.anchorX}px`,
            top: `${contextMenu.state.anchorY}px`,
          }"
        />
      </DropdownMenuTrigger>
      <DropdownMenuContent
        side="bottom"
        align="start"
        :side-offset="0"
        :align-offset="0"
        @contextmenu.prevent.stop
        @escape-key-down="contextMenu.setOpen(false)"
        @pointer-down-outside="contextMenu.setOpen(false)"
      >
        <GalleryAssetDropdownMenuContent />
      </DropdownMenuContent>
    </DropdownMenu>
  </div>
</template>
