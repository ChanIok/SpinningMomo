
import { defineStore } from 'pinia'
import { ref } from 'vue'
import { settingsApi } from './api'
import type { AppSettings } from './types'
import { DEFAULT_APP_SETTINGS } from './types'

export const useSettingsStore = defineStore('settings', () => {
  const appSettings = ref<AppSettings>(DEFAULT_APP_SETTINGS)
  const isLoading = ref(false)
  const error = ref<string | null>(null)
  const isInitialized = ref(false)

  const init = async () => {
    if (isInitialized.value) return

    isLoading.value = true
    error.value = null
    try {
      const settings = await settingsApi.get()
      appSettings.value = settings
      isInitialized.value = true
    } catch (e) {
      error.value = (e as Error).message
    } finally {
      isLoading.value = false
    }
  }

  const updateSettings = async (newSettings: Partial<AppSettings>) => {
    // 乐观更新
    const oldSettings = JSON.parse(JSON.stringify(appSettings.value))
    
    // 深度合并 (简单实现，主要用于合并顶层 keys)
    // 注意：这里需要根据实际情况决定是完全替换还是合并。
    // React 版本中是替换整个 appSettings 对象，但 API update 接收 AppSettings
    // 我们假设传入的是完整的 AppSettings 更新，或者是做了合并后的结果
    
    // 为了安全起见，我们假设传入的是 AppSettings 的一部分，并在 store 内部做合并
    // 但 AppSettings 类型比较复杂，typescript deep merge 比较麻烦
    // 我们约定调用者必须传入完整的 AppSettings (或者在 Action 中处理合并)
    
    // 修正：让我们跟 React 保持一致，React 的 useSettingsStore updateSettings 接收 updates
    // 但这里的 API update 接收 AppSettings。
    
    const nextSettings = { ...appSettings.value, ...newSettings }
    appSettings.value = nextSettings as AppSettings

    try {
      await settingsApi.update(nextSettings as AppSettings)
    } catch (e) {
      // 回滚
      appSettings.value = oldSettings
      error.value = (e as Error).message
      throw e
    }
  }
  
  const clearError = () => {
    error.value = null
  }

  return {
    appSettings,
    isLoading,
    error,
    isInitialized,
    init,
    updateSettings,
    clearError
  }
})
