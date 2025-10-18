<script setup lang="ts">
import { onMounted } from 'vue'
import { useGalleryStore } from '../store'
import { useGalleryData } from '../composables'
import GalleryLayout from '../layout/GalleryLayout.vue'

const store = useGalleryStore()
const galleryData = useGalleryData()

onMounted(() => {
  if (store.assets.length === 0 && store.totalCount === 0) {
    galleryData.initialize()
  }
})
</script>

<template>
  <GalleryLayout />
</template>
