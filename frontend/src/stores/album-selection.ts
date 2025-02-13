import { defineStore } from 'pinia'

interface SelectionState {
  selectedIds: Set<number>
  isSelectionMode: boolean
}

export const useAlbumSelectionStore = defineStore('album-selection', {
  state: (): SelectionState => ({
    selectedIds: new Set<number>(),
    isSelectionMode: false
  }),

  getters: {
    selectedCount(): number {
      return this.selectedIds.size
    },

    isSelected(): (id: number) => boolean {
      return (id: number) => this.selectedIds.has(id)
    }
  },

  actions: {
    toggleSelection(id: number) {
      if (this.selectedIds.has(id)) {
        this.selectedIds.delete(id)
        if (this.selectedIds.size === 0) {
          this.isSelectionMode = false
        }
      } else {
        this.selectedIds.add(id)
        if (!this.isSelectionMode) {
          this.isSelectionMode = true
        }
      }
    },

    selectMultiple(ids: number[]) {
      ids.forEach(id => this.selectedIds.add(id))
      if (ids.length > 0) {
        this.isSelectionMode = true
      }
    },

    deselectMultiple(ids: number[]) {
      ids.forEach(id => this.selectedIds.delete(id))
      if (this.selectedIds.size === 0) {
        this.isSelectionMode = false
      }
    },

    selectAll(ids: number[]) {
      ids.forEach(id => this.selectedIds.add(id))
      this.isSelectionMode = true
    },

    clearSelection() {
      this.selectedIds.clear()
      this.isSelectionMode = false
    },

    enterSelectionMode() {
      this.isSelectionMode = true
    },

    exitSelectionMode() {
      this.selectedIds.clear()
      this.isSelectionMode = false
    }
  }
}) 