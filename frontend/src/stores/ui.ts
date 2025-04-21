import { defineStore } from 'pinia';
import { ref } from 'vue';

export const useUIStore = defineStore('ui', () => {
  // State
  const viewMode = ref<'grid' | 'list'>('grid');

  // Actions
  function setViewMode(mode: 'grid' | 'list') {
    viewMode.value = mode;
  }

  function toggleViewMode() {
    viewMode.value = viewMode.value === 'grid' ? 'list' : 'grid';
  }

  return {
    // State
    viewMode,
    
    // Actions
    setViewMode,
    toggleViewMode
  };
});
