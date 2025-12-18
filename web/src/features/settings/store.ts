import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { settingsApi } from './api'
import type { AppSettings } from './types'
import { DEFAULT_APP_SETTINGS } from './types'
import { useI18n } from '@/composables/useI18n'
import type { Locale } from '@/core/i18n/types'

export const useSettingsStore = defineStore('settings', () => {
  const appSettings = ref<AppSettings>(DEFAULT_APP_SETTINGS)
  const isLoading = ref(false)
  const error = ref<string | null>(null)
  const isInitialized = ref(false)
  const { setLocale } = useI18n()

  const init = async () => {
    if (isInitialized.value) return

    isLoading.value = true
    error.value = null
    try {
      const settings = await settingsApi.get()
      appSettings.value = settings

      // 同步语言设置到 i18n
      const language = settings.app.language.current as Locale
      await setLocale(language)

      isInitialized.value = true
    } catch (e) {
      error.value = (e as Error).message
    } finally {
      isLoading.value = false
    }
  }

  // 监听语言设置变化，自动同步到 i18n
  watch(
    () => appSettings.value.app.language.current,
    async (newLanguage) => {
      if (isInitialized.value) {
        await setLocale(newLanguage as Locale)
      }
    }
  )

  const updateSettings = async (newSettings: Partial<AppSettings>) => {
    // 乐观更新
    const oldSettings = JSON.parse(JSON.stringify(appSettings.value))

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
    clearError,
  }
})
