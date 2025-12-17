
import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { WebSettings } from './types'
import { DEFAULT_WEB_SETTINGS } from './types'
import {
  readWebSettings,
  writeWebSettings,
  initializeWebSettings,
} from './api'

export const useWebSettingsStore = defineStore('web-settings', () => {
  const webSettings = ref<WebSettings>(DEFAULT_WEB_SETTINGS)
  const error = ref<string | null>(null)
  const isInitialized = ref(false)

  const setSettings = (settings: WebSettings) => {
    webSettings.value = settings
  }

  const setError = (err: string | null) => {
    error.value = err
  }

  const clearError = () => {
    error.value = null
  }

  const initialize = async () => {
    if (isInitialized.value) return

    try {
      error.value = null
      const settings = await initializeWebSettings()
      webSettings.value = settings
      isInitialized.value = true
      console.log('✅ 前端设置 Store 初始化完成')
    } catch (err) {
       error.value = err instanceof Error ? err.message : '初始化失败'
       isInitialized.value = true // Avoid infinite loop
       console.error('❌ 前端设置 Store 初始化失败:', err)
    }
  }

  const loadSettings = async () => {
    try {
      error.value = null
      const settings = await readWebSettings()
      if (settings) {
        webSettings.value = settings
      } else {
        webSettings.value = DEFAULT_WEB_SETTINGS
      }
    } catch (err) {
      error.value = err instanceof Error ? err.message : '加载设置失败'
      throw err
    }
  }

  const updateSettings = async (partialSettings: Partial<WebSettings>) => {
    const previousSettings = JSON.parse(JSON.stringify(webSettings.value))

    // Optimistic update
    const optimisticSettings = {
        ...webSettings.value,
        ...partialSettings,
        ui: {
            ...webSettings.value.ui,
            ...(partialSettings.ui || {}),
            background: {
                ...webSettings.value.ui.background,
                ...(partialSettings.ui?.background || {}),
            },
            theme: {
                ...webSettings.value.ui.theme,
                ...(partialSettings.ui?.theme || {}),
            }
        }
    }

    webSettings.value = optimisticSettings
    error.value = null

    try {
        await writeWebSettings(optimisticSettings)
        console.log('✅ Web设置已更新:', partialSettings)
    } catch (err) {
        webSettings.value = previousSettings
        error.value = err instanceof Error ? err.message : '更新Web设置失败'
        console.error('❌ Web设置更新失败，已回滚:', err)
        throw err
    }
  }

  const resetToDefault = async () => {
      try {
          const defaultSettings = {
              ...DEFAULT_WEB_SETTINGS,
              createdAt: webSettings.value.createdAt,
              updatedAt: new Date().toISOString()
          }
          await writeWebSettings(defaultSettings)
          webSettings.value = defaultSettings
          console.log('✅ 已重置为默认设置')
      } catch (err) {
          console.error('重置设置失败:', err)
          throw err
      }
  }

  return {
    webSettings,
    error,
    isInitialized,
    setSettings,
    setError,
    clearError,
    initialize,
    loadSettings,
    updateSettings,
    resetToDefault
  }
})
