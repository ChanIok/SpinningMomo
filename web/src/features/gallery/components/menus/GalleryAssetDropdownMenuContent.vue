<script setup lang="ts">
import { useI18n } from '@/composables/useI18n'
import {
  DropdownMenuItem,
  DropdownMenuSeparator,
  DropdownMenuShortcut,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
} from '@/components/ui/dropdown-menu'
import { useGalleryAssetActions } from '../../composables'

const { t } = useI18n()
const assetActions = useGalleryAssetActions()
</script>

<template>
  <DropdownMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleOpenAssetDefault"
  >
    {{ t('gallery.contextMenu.openDefaultApp.label') }}
  </DropdownMenuItem>
  <DropdownMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleRevealAssetInExplorer"
  >
    {{ t('gallery.contextMenu.revealInExplorer.label') }}
  </DropdownMenuItem>
  <DropdownMenuSeparator />
  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      {{ t('gallery.contextMenu.review.rating.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRating(1)">
        1★
        <DropdownMenuShortcut>1</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRating(2)">
        2★
        <DropdownMenuShortcut>2</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRating(3)">
        3★
        <DropdownMenuShortcut>3</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRating(4)">
        4★
        <DropdownMenuShortcut>4</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRating(5)">
        5★
        <DropdownMenuShortcut>5</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuSeparator />
      <DropdownMenuItem @click="assetActions.clearSelectedAssetsRating">
        {{ t('gallery.contextMenu.review.rating.clear') }}
        <DropdownMenuShortcut>0</DropdownMenuShortcut>
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      {{ t('gallery.contextMenu.review.flag.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRejected()">
        {{ t('gallery.review.flag.rejected') }}
        <DropdownMenuShortcut>X</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuSeparator />
      <DropdownMenuItem @click="assetActions.clearSelectedAssetsRejected">
        {{ t('gallery.contextMenu.review.flag.clear') }}
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
  <DropdownMenuSeparator />
  <DropdownMenuItem
    class="text-destructive focus:text-destructive"
    @click="assetActions.handleMoveAssetsToTrash"
  >
    {{ t('gallery.contextMenu.moveToTrash.label') }}
  </DropdownMenuItem>
</template>
