<script setup lang="ts">
import { computed } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { isLocalAccess } from '@/core/access'
import { Copy, Eraser, ExternalLink, FolderOpen, Star, Trash2, X } from '@lucide/vue'
import {
  DropdownMenuItem,
  DropdownMenuSeparator,
  DropdownMenuShortcut,
  DropdownMenuSub,
  DropdownMenuSubContent,
  DropdownMenuSubTrigger,
} from '@/components/ui/dropdown-menu'
import { Kbd, KbdGroup } from '@/components/ui/kbd'
import { useGalleryAssetActions } from '../../composables'
import GalleryPasteDropdownMenuItem from './GalleryPasteDropdownMenuItem.vue'

const { t } = useI18n()
const assetActions = useGalleryAssetActions()
// 下拉菜单与右键菜单共享同一套宿主机能力边界。
const canUseLocalFileSystem = computed(() => isLocalAccess())
const ratingOptions = [1, 2, 3, 4, 5] as const
</script>

<template>
  <!-- 远端隐藏文件打开、资源管理器和系统剪贴板入口。 -->
  <template v-if="canUseLocalFileSystem">
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
      <DropdownMenuShortcut>
        <KbdGroup>
          <Kbd>Ctrl</Kbd>
          <Kbd>C</Kbd>
        </KbdGroup>
      </DropdownMenuShortcut>
    </DropdownMenuItem>
  </template>
  <GalleryPasteDropdownMenuItem />
  <DropdownMenuItem
    inset
    :disabled="!assetActions.hasSelection"
    @click="assetActions.openMoveToFolderDialog"
  >
    {{ t('gallery.contextMenu.moveToFolder.label') }}
  </DropdownMenuItem>
  <DropdownMenuSeparator />
  <DropdownMenuItem
    inset
    :disabled="!assetActions.canCopyTags"
    @click="assetActions.copySelectedAssetTags"
  >
    {{ t('gallery.contextMenu.copyTags.label') }}
    <DropdownMenuShortcut>
      <KbdGroup>
        <Kbd>Ctrl</Kbd>
        <Kbd>Shift</Kbd>
        <Kbd>C</Kbd>
      </KbdGroup>
    </DropdownMenuShortcut>
  </DropdownMenuItem>
  <DropdownMenuItem
    inset
    :disabled="!assetActions.canPasteTags"
    @click="assetActions.pasteCopiedTagsToSelection"
  >
    {{ t('gallery.contextMenu.pasteTags.label') }}
    <DropdownMenuShortcut>
      <KbdGroup>
        <Kbd>Ctrl</Kbd>
        <Kbd>Shift</Kbd>
        <Kbd>V</Kbd>
      </KbdGroup>
    </DropdownMenuShortcut>
  </DropdownMenuItem>
  <DropdownMenuSeparator />
  <DropdownMenuSub>
    <DropdownMenuSubTrigger inset>
      {{ t('gallery.contextMenu.review.rating.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem
        v-for="rating in ratingOptions"
        :key="rating"
        @click="assetActions.setSelectedAssetsRating(rating)"
      >
        <span class="flex items-center gap-0.5 text-muted-foreground">
          <Star v-for="index in rating" :key="`${rating}-${index}`" class="fill-current" />
        </span>
        <DropdownMenuShortcut>
          <Kbd>{{ rating }}</Kbd>
        </DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuSeparator />
      <DropdownMenuItem @click="assetActions.clearSelectedAssetsRating">
        <Eraser />
        {{ t('gallery.contextMenu.review.rating.clear') }}
        <DropdownMenuShortcut>
          <Kbd>0</Kbd>
        </DropdownMenuShortcut>
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
  <DropdownMenuSub>
    <DropdownMenuSubTrigger inset>
      {{ t('gallery.contextMenu.review.flag.label') }}
    </DropdownMenuSubTrigger>
    <DropdownMenuSubContent class="w-40">
      <DropdownMenuItem @click="assetActions.setSelectedAssetsRejected()">
        <X />
        {{ t('gallery.review.flag.rejected') }}
        <DropdownMenuShortcut>
          <Kbd>X</Kbd>
        </DropdownMenuShortcut>
      </DropdownMenuItem>
      <DropdownMenuSeparator />
      <DropdownMenuItem @click="assetActions.clearSelectedAssetsRejected">
        <Eraser />
        {{ t('gallery.contextMenu.review.flag.clear') }}
      </DropdownMenuItem>
    </DropdownMenuSubContent>
  </DropdownMenuSub>
  <DropdownMenuSeparator />
  <DropdownMenuItem variant="destructive" @click="assetActions.requestDeleteAssets">
    <Trash2 />
    {{ assetActions.deleteMenuLabel }}
  </DropdownMenuItem>
</template>
