import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { Album } from '@/types/album'
import { albumAPI } from '@/api/album'

export const useAlbumStore = defineStore('album', () => {
  // State
  const albums = ref<Album[]>([])
  const loading = ref(false)
  const editingAlbum = ref<Album | null>(null)

  // Actions
  async function loadAlbums() {
    try {
      loading.value = true
      albums.value = await albumAPI.getAlbums()
    } catch (error) {
      throw error
    } finally {
      loading.value = false
    }
  }

  async function createAlbum(data: { name: string; description?: string }) {
    try {
      loading.value = true
      const album = await albumAPI.createAlbum(data)
      albums.value.push(album)
      return album
    } catch (error) {
      throw error
    } finally {
      loading.value = false
    }
  }

  async function updateAlbum(id: number, data: Partial<Album>) {
    try {
      loading.value = true
      const updated = await albumAPI.updateAlbum(id, data)
      const index = albums.value.findIndex(a => a.id === id)
      if (index !== -1) {
        albums.value[index] = updated
      }
      return updated
    } catch (error) {
      throw error
    } finally {
      loading.value = false
    }
  }

  async function deleteAlbum(id: number) {
    try {
      loading.value = true
      await albumAPI.deleteAlbum(id)
      albums.value = albums.value.filter(a => a.id !== id)
    } catch (error) {
      throw error
    } finally {
      loading.value = false
    }
  }

  async function deleteAlbums(ids: number[]) {
    try {
      loading.value = true
      await Promise.all(ids.map(id => albumAPI.deleteAlbum(id)))
      albums.value = albums.value.filter(a => !ids.includes(a.id))
    } catch (error) {
      throw error
    } finally {
      loading.value = false
    }
  }

  function setEditingAlbum(album: Album | null) {
    editingAlbum.value = album
  }

  function reset() {
    albums.value = []
    loading.value = false
    editingAlbum.value = null
  }

  return {
    // State
    albums,
    loading,
    editingAlbum,

    // Actions
    loadAlbums,
    createAlbum,
    updateAlbum,
    deleteAlbum,
    deleteAlbums,
    setEditingAlbum,
    reset
  }
}) 