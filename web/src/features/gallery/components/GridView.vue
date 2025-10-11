<script setup lang="ts">
import { useGalleryView, useGallerySelection } from '../composables'
import AssetCard from './AssetCard.vue'

const galleryView = useGalleryView()
const gallerySelection = useGallerySelection()

function handleAssetClick(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetClick(asset, event, galleryView.sortedAssets.value)
}

function handleAssetDoubleClick(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetDoubleClick(asset, event)
  // TODO: 打开 lightbox 预览
  console.log('打开预览:', asset.name)
}

function handleAssetContextMenu(asset: any, event: MouseEvent) {
  gallerySelection.handleAssetContextMenu(asset, event)
}

function handleAssetPreview(asset: any) {
  console.log('预览资产:', asset.name)
}
</script>

<template>
  <div class="h-full overflow-auto p-6">
    <div
      class="grid justify-items-center gap-4"
      :style="{
        gridTemplateColumns: `repeat(auto-fill, minmax(${galleryView.viewSize.value}px, 1fr))`,
      }"
    >
      <AssetCard
        v-for="asset in galleryView.sortedAssets.value"
        :key="asset.id"
        :asset="asset"
        :is-selected="gallerySelection.isAssetSelected(asset.id)"
        :is-active="gallerySelection.isAssetActive(asset.id)"
        :show-name="galleryView.viewSize.value >= 200"
        :show-size="galleryView.viewSize.value >= 280"
        :show-type-label="galleryView.viewSize.value >= 280"
        @click="handleAssetClick"
        @double-click="handleAssetDoubleClick"
        @context-menu="handleAssetContextMenu"
        @preview="handleAssetPreview"
      />
    </div>
  </div>
</template>