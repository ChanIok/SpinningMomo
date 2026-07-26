<script setup lang="ts">
import { ClipboardPaste } from 'lucide-vue-next'
import { DropdownMenuItem, DropdownMenuShortcut } from '@/components/ui/dropdown-menu'
import { Kbd, KbdGroup } from '@/components/ui/kbd'
import { useI18n } from '@/composables/useI18n'
import { useGalleryFolderActions } from '../../composables'

const { t } = useI18n()
const folderActions = useGalleryFolderActions()
const selectedFolderId = folderActions.selectedFolderId
</script>

<template>
  <DropdownMenuItem
    :disabled="selectedFolderId === undefined"
    @click="folderActions.pasteClipboardToSelectedFolder"
  >
    <ClipboardPaste />
    {{ t('gallery.contextMenu.pasteFiles.label') }}
    <DropdownMenuShortcut>
      <KbdGroup>
        <Kbd>Ctrl</Kbd>
        <Kbd>V</Kbd>
      </KbdGroup>
    </DropdownMenuShortcut>
  </DropdownMenuItem>
</template>
