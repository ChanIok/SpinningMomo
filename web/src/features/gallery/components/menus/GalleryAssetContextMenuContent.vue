<script setup lang="ts">
import { useI18n } from '@/composables/useI18n'
import { Eraser, ExternalLink, Flag, FolderOpen, Star, Trash2, X } from 'lucide-vue-next'
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
const ratingOptions = [1, 2, 3, 4, 5] as const
</script>

<template>
  <ContextMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleOpenAssetDefault"
  >
    <ExternalLink />
    {{ t('gallery.contextMenu.openDefaultApp.label') }}
  </ContextMenuItem>
  <ContextMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleRevealAssetInExplorer"
  >
    <FolderOpen />
    {{ t('gallery.contextMenu.revealInExplorer.label') }}
  </ContextMenuItem>
  <ContextMenuSeparator />
  <ContextMenuSub>
    <ContextMenuSubTrigger>
      <Star />
      {{ t('gallery.contextMenu.review.rating.label') }}
    </ContextMenuSubTrigger>
    <ContextMenuSubContent class="w-40">
      <ContextMenuItem
        v-for="rating in ratingOptions"
        :key="rating"
        @click="assetActions.setSelectedAssetsRating(rating)"
      >
        <span class="flex items-center gap-0.5">
          <Star v-for="index in rating" :key="`${rating}-${index}`" class="fill-current" />
        </span>
        <ContextMenuShortcut>{{ rating }}</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuSeparator />
      <ContextMenuItem @click="assetActions.clearSelectedAssetsRating">
        <Eraser />
        {{ t('gallery.contextMenu.review.rating.clear') }}
        <ContextMenuShortcut>0</ContextMenuShortcut>
      </ContextMenuItem>
    </ContextMenuSubContent>
  </ContextMenuSub>
  <ContextMenuSub>
    <ContextMenuSubTrigger>
      <Flag />
      {{ t('gallery.contextMenu.review.flag.label') }}
    </ContextMenuSubTrigger>
    <ContextMenuSubContent class="w-40">
      <ContextMenuItem @click="assetActions.setSelectedAssetsRejected()">
        <X />
        {{ t('gallery.review.flag.rejected') }}
        <ContextMenuShortcut>X</ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuSeparator />
      <ContextMenuItem @click="assetActions.clearSelectedAssetsRejected">
        <Eraser />
        {{ t('gallery.contextMenu.review.flag.clear') }}
      </ContextMenuItem>
    </ContextMenuSubContent>
  </ContextMenuSub>
  <ContextMenuSeparator />
  <ContextMenuItem variant="destructive" @click="assetActions.handleMoveAssetsToTrash">
    <Trash2 />
    {{ t('gallery.contextMenu.moveToTrash.label') }}
  </ContextMenuItem>
</template>
