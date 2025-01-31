import { defineStore } from 'pinia'
import { useOsTheme } from 'naive-ui'
import { ref, computed } from 'vue'

export const useThemeStore = defineStore('theme', () => {
  const osTheme = useOsTheme()
  const isDark = ref(osTheme.value === 'dark')

  const toggleTheme = () => {
    isDark.value = !isDark.value
  }

  return {
    isDark,
    toggleTheme
  }
}) 