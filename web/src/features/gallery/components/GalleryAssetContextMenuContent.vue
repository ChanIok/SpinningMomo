<script setup lang="ts">
import { useI18n } from '@/composables/useI18n'
import { ContextMenuItem, ContextMenuSeparator } from '@/components/ui/context-menu'
import { useGalleryAssetActions } from '../composables'

const { t } = useI18n()
const assetActions = useGalleryAssetActions()
</script>

<template>
  <ContextMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleOpenAssetDefault"
  >
    {{ t('gallery.contextMenu.openDefaultApp.label') }}
  </ContextMenuItem>
  <ContextMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleRevealAssetInExplorer"
  >
    {{ t('gallery.contextMenu.revealInExplorer.label') }}
  </ContextMenuItem>
  <ContextMenuSeparator />
  <ContextMenuItem
    class="text-destructive focus:text-destructive"
    @click="assetActions.handleMoveAssetsToTrash"
  >
    {{ t('gallery.contextMenu.moveToTrash.label') }}
  </ContextMenuItem>
</template>
