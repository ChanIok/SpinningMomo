<script setup lang="ts">
import { computed } from 'vue'
import { ClipboardPaste } from '@lucide/vue'
import { DropdownMenuItem, DropdownMenuShortcut } from '@/components/ui/dropdown-menu'
import { Kbd, KbdGroup } from '@/components/ui/kbd'
import { useI18n } from '@/composables/useI18n'
import { useGalleryFolderActions } from '../../composables'
import { isLocalAccess } from '@/core/access'

const { t } = useI18n()
const folderActions = useGalleryFolderActions()
const selectedFolderId = folderActions.selectedFolderId
// 系统剪贴板粘贴只在本机 WebView 中提供。
const canUseLocalFileSystem = computed(() => isLocalAccess())
</script>

<template>
  <DropdownMenuItem
    v-if="canUseLocalFileSystem"
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
