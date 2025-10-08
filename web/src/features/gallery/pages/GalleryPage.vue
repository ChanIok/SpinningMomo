<script setup lang="ts">
import { onMounted } from 'vue'
import { useGalleryData } from '../composables'
import GalleryLayout from '../layout/GalleryLayout.vue'

const galleryData = useGalleryData()

onMounted(() => {
  if (galleryData.assets.value.length === 0) {
    galleryData.initialize()
  }
})
</script>

<template>
  <GalleryLayout />
</template>