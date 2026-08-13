<script setup lang="ts">
import { nextTick, watch } from 'vue'
import { useGalleryStore } from '../../store'
import GalleryAssetDropdownMenuContent from './GalleryAssetDropdownMenuContent.vue'
import GalleryBackgroundDropdownMenuContent from './GalleryBackgroundDropdownMenuContent.vue'
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuTrigger,
} from '@/components/ui/dropdown-menu'

const store = useGalleryStore()

watch(
  () => store.contextMenu.requestToken,
  async (token) => {
    if (token <= 0) {
      return
    }

    // 等待锚点位移先提交到 DOM，再以受控方式打开菜单，避免定位闪动。
    await nextTick()
    store.setContextMenuOpen(true)
  }
)
</script>

<template>
  <div>
    <DropdownMenu
      :open="store.contextMenu.isOpen"
      :modal="false"
      @update:open="store.setContextMenuOpen"
    >
      <DropdownMenuTrigger as-child>
        <div
          class="pointer-events-none fixed h-px w-px opacity-0"
          :style="{
            left: `${store.contextMenu.anchorX}px`,
            top: `${store.contextMenu.anchorY}px`,
          }"
        />
      </DropdownMenuTrigger>
      <DropdownMenuContent
        side="bottom"
        align="start"
        :side-offset="0"
        :align-offset="0"
        @contextmenu.prevent.stop
        @escape-key-down="store.setContextMenuOpen(false)"
        @pointer-down-outside="store.setContextMenuOpen(false)"
      >
        <GalleryAssetDropdownMenuContent v-if="store.contextMenu.target === 'asset'" />
        <GalleryBackgroundDropdownMenuContent v-else />
      </DropdownMenuContent>
    </DropdownMenu>
  </div>
</template>
