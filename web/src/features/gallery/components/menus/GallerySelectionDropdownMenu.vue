<script setup lang="ts">
import { ListTodo } from '@lucide/vue'
import {
  DropdownMenuItem,
  DropdownMenuShortcut,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
} from '@/components/ui/dropdown-menu'
import { Kbd, KbdGroup } from '@/components/ui/kbd'
import { useI18n } from '@/composables/useI18n'
import { useGallerySelection } from '../../composables'

const { t } = useI18n()
const gallerySelection = useGallerySelection()
</script>

<template>
  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      <ListTodo />
      {{ t('gallery.contextMenu.selection.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem @click="gallerySelection.selectAllCurrentQuery">
        {{ t('gallery.contextMenu.selection.selectAll') }}
        <DropdownMenuShortcut>
          <KbdGroup>
            <Kbd>Ctrl</Kbd>
            <Kbd>A</Kbd>
          </KbdGroup>
        </DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuItem @click="gallerySelection.invertCurrentQuery">
        {{ t('gallery.contextMenu.selection.invert') }}
      </DropdownMenuItem>
      <DropdownMenuItem
        :disabled="!gallerySelection.hasSelection"
        @click="gallerySelection.clearSelection"
      >
        {{ t('gallery.contextMenu.selection.clear') }}
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
</template>
