import { defineStore } from 'pinia'

interface SelectionState {
  selectedIds: Set<number>
  isSelectionMode: boolean
}

export const useSelectionStore = defineStore('screenshot-selection', {
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
      }
    },

    enterSelectionMode() {
      this.isSelectionMode = true
    },

    exitSelectionMode() {
      this.isSelectionMode = false
      this.selectedIds.clear()
    },

    clearSelection() {
      this.selectedIds.clear()
    }
  }
}) 