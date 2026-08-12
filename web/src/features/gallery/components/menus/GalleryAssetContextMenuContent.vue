<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { isLocalAccess } from '@/core/access'
import { Copy, Eraser, ExternalLink, FolderOpen, Star, Trash2, X } from '@lucide/vue'
import {
  ContextMenuItem,
  ContextMenuSeparator,
  ContextMenuShortcut,
  ContextMenuSub,
  ContextMenuSubContent,
  ContextMenuSubTrigger,
} from '@/components/ui/context-menu'
import { Kbd, KbdGroup } from '@/components/ui/kbd'
import { useGalleryAssetActions } from '../../composables'

const { t } = useI18n()
const assetActions = useGalleryAssetActions()
// 打开默认程序、资源管理器和系统剪贴板都属于 local 能力。
const canUseLocalFileSystem = computed(() => isLocalAccess())
const ratingOptions = [1, 2, 3, 4, 5] as const
</script>

<template>
  <!-- LAN 仍可管理图库标签，但不展示宿主机文件操作。 -->
  <template v-if="canUseLocalFileSystem">
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
    <ContextMenuItem
      :disabled="!assetActions.hasSelection"
      @click="assetActions.handleCopyAssetsToClipboard"
    >
      <Copy />
      {{ t('gallery.contextMenu.copyFiles.label') }}
      <ContextMenuShortcut>
        <KbdGroup>
          <Kbd>Ctrl</Kbd>
          <Kbd>C</Kbd>
        </KbdGroup>
      </ContextMenuShortcut>
    </ContextMenuItem>
  </template>
  <ContextMenuItem
    inset
    :disabled="!assetActions.hasSelection"
    @click="assetActions.openMoveToFolderDialog"
  >
    {{ t('gallery.contextMenu.moveToFolder.label') }}
  </ContextMenuItem>
  <ContextMenuSeparator />
  <ContextMenuItem
    inset
    :disabled="!assetActions.canCopyTags"
    @click="assetActions.copySelectedAssetTags"
  >
    {{ t('gallery.contextMenu.copyTags.label') }}
    <ContextMenuShortcut>
      <KbdGroup>
        <Kbd>Ctrl</Kbd>
        <Kbd>Shift</Kbd>
        <Kbd>C</Kbd>
      </KbdGroup>
    </ContextMenuShortcut>
  </ContextMenuItem>
  <ContextMenuItem
    inset
    :disabled="!assetActions.canPasteTags"
    @click="assetActions.pasteCopiedTagsToSelection"
  >
    {{ t('gallery.contextMenu.pasteTags.label') }}
    <ContextMenuShortcut>
      <KbdGroup>
        <Kbd>Ctrl</Kbd>
        <Kbd>Shift</Kbd>
        <Kbd>V</Kbd>
      </KbdGroup>
    </ContextMenuShortcut>
  </ContextMenuItem>
  <ContextMenuSeparator />
  <ContextMenuSub>
    <ContextMenuSubTrigger inset>
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
        <ContextMenuShortcut>
          <Kbd>{{ rating }}</Kbd>
        </ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuSeparator />
      <ContextMenuItem @click="assetActions.clearSelectedAssetsRating">
        <Eraser />
        {{ t('gallery.contextMenu.review.rating.clear') }}
        <ContextMenuShortcut>
          <Kbd>0</Kbd>
        </ContextMenuShortcut>
      </ContextMenuItem>
    </ContextMenuSubContent>
  </ContextMenuSub>
  <ContextMenuSub>
    <ContextMenuSubTrigger inset>
      {{ t('gallery.contextMenu.review.flag.label') }}
    </ContextMenuSubTrigger>
    <ContextMenuSubContent class="w-40">
      <ContextMenuItem @click="assetActions.setSelectedAssetsRejected()">
        <X />
        {{ t('gallery.review.flag.rejected') }}
        <ContextMenuShortcut>
          <Kbd>X</Kbd>
        </ContextMenuShortcut>
      </ContextMenuItem>
      <ContextMenuSeparator />
      <ContextMenuItem @click="assetActions.clearSelectedAssetsRejected">
        <Eraser />
        {{ t('gallery.contextMenu.review.flag.clear') }}
      </ContextMenuItem>
    </ContextMenuSubContent>
  </ContextMenuSub>
  <ContextMenuSeparator />
  <ContextMenuItem variant="destructive" @click="assetActions.requestDeleteAssets">
    <Trash2 />
    {{ assetActions.deleteMenuLabel }}
  </ContextMenuItem>
</template>
