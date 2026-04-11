<script setup lang="ts">
import { useI18n } from '@/composables/useI18n'
import {
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuShortcut,
  ContextMenuSub,
  ContextMenuSubContent,
  ContextMenuSubTrigger,
} from '@/components/ui/context-menu'
import { useGalleryAssetActions } from '../../composables'

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
  <ContextMenuSub>
    <ContextMenuSubTrigger>
      {{ t('gallery.contextMenu.review.rating.label') }}
    </ContextMenuSubTrigger>
    <ContextMenuSubContent class="w-40">
      <ContextMenuItem @click="assetActions.setSelectedAssetsRating(1)">
        1★
        <ContextMenuShortcut>1</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuItem @click="assetActions.setSelectedAssetsRating(2)">
        2★
        <ContextMenuShortcut>2</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuItem @click="assetActions.setSelectedAssetsRating(3)">
        3★
        <ContextMenuShortcut>3</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuItem @click="assetActions.setSelectedAssetsRating(4)">
        4★
        <ContextMenuShortcut>4</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuItem @click="assetActions.setSelectedAssetsRating(5)">
        5★
        <ContextMenuShortcut>5</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuSeparator />
      <ContextMenuItem @click="assetActions.clearSelectedAssetsRating">
        {{ t('gallery.contextMenu.review.rating.clear') }}
        <ContextMenuShortcut>0</ContextMenuShortcut>
      </ContextMenuItem>
    </ContextMenuSubContent>
  </ContextMenuSub>
  <ContextMenuSub>
    <ContextMenuSubTrigger>
      {{ t('gallery.contextMenu.review.flag.label') }}
    </ContextMenuSubTrigger>
    <ContextMenuSubContent class="w-40">
      <ContextMenuItem @click="assetActions.setSelectedAssetsRejected()">
        {{ t('gallery.review.flag.rejected') }}
        <ContextMenuShortcut>X</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuSeparator />
      <ContextMenuItem @click="assetActions.clearSelectedAssetsRejected">
        {{ t('gallery.contextMenu.review.flag.clear') }}
      </ContextMenuItem>
    </ContextMenuSubContent>
  </ContextMenuSub>
  <ContextMenuSeparator />
  <ContextMenuItem
    class="text-destructive focus:text-destructive"
    @click="assetActions.handleMoveAssetsToTrash"
  >
    {{ t('gallery.contextMenu.moveToTrash.label') }}
  </ContextMenuItem>
</template>
