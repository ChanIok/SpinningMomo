<script setup lang="ts">
import { useI18n } from '@/composables/useI18n'
import { Copy, Eraser, ExternalLink, Flag, FolderOpen, Star, Trash2, X } from 'lucide-vue-next'
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
const ratingOptions = [1, 2, 3, 4, 5] as const
</script>

<template>
  <DropdownMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleOpenAssetDefault"
  >
    <ExternalLink />
    {{ t('gallery.contextMenu.openDefaultApp.label') }}
  </DropdownMenuItem>
  <DropdownMenuItem
    :disabled="!assetActions.isSingleSelection"
    @click="assetActions.handleRevealAssetInExplorer"
  >
    <FolderOpen />
    {{ t('gallery.contextMenu.revealInExplorer.label') }}
  </DropdownMenuItem>
  <DropdownMenuSeparator />
  <DropdownMenuItem
    :disabled="!assetActions.hasSelection"
    @click="assetActions.handleCopyAssetsToClipboard"
  >
    <Copy />
    {{ t('gallery.contextMenu.copyFiles.label') }}
  </DropdownMenuItem>
  <DropdownMenuSeparator />
  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      <Star />
      {{ t('gallery.contextMenu.review.rating.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem
        v-for="rating in ratingOptions"
        :key="rating"
        @click="assetActions.setSelectedAssetsRating(rating)"
      >
        <span class="flex items-center gap-0.5">
          <Star v-for="index in rating" :key="`${rating}-${index}`" class="fill-current" />
        </span>
        <DropdownMenuShortcut>{{ rating }}</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuSeparator />
      <DropdownMenuItem @click="assetActions.clearSelectedAssetsRating">
        <Eraser />
        {{ t('gallery.contextMenu.review.rating.clear') }}
        <DropdownMenuShortcut>0</DropdownMenuShortcut>
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
  <DropdownMenuSub>
    <DropdownMenuSubTrigger>
      <Flag />
      {{ t('gallery.contextMenu.review.flag.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRejected()">
        <X />
        {{ t('gallery.review.flag.rejected') }}
        <DropdownMenuShortcut>X</DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuSeparator />
      <DropdownMenuItem @click="assetActions.clearSelectedAssetsRejected">
        <Eraser />
        {{ t('gallery.contextMenu.review.flag.clear') }}
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
  <DropdownMenuSeparator />
  <DropdownMenuItem variant="destructive" @click="assetActions.handleMoveAssetsToTrash">
    <Trash2 />
    {{ t('gallery.contextMenu.moveToTrash.label') }}
  </DropdownMenuItem>
</template>
