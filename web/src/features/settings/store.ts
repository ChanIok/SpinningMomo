import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { settingsApi } from './api'
import type { AppSettings } from './types'
import { DEFAULT_APP_SETTINGS } from './types'
import { useI18n } from '@/composables/useI18n'
import type { Locale } from '@/core/i18n/types'
import { on, off } from '@/core/rpc'

type JsonObject = Record<string, unknown>
const NO_CHANGE = Symbol('no_change')

const isPlainObject = (value: unknown): value is JsonObject => {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

const cloneDeep = <T>(value: T): T => {
  return JSON.parse(JSON.stringify(value)) as T
}

const deepMerge = <T>(base: T, patch: unknown): T => {
  if (!isPlainObject(base) || !isPlainObject(patch)) {
    return patch as T
  }

  const merged: JsonObject = { ...(base as JsonObject) }
  for (const [key, patchValue] of Object.entries(patch)) {
    const baseValue = (base as JsonObject)[key]
    if (isPlainObject(baseValue) && isPlainObject(patchValue)) {
      merged[key] = deepMerge(baseValue, patchValue)
      continue
    }
    merged[key] = patchValue
  }

  return merged as T
}

const buildPatch = (before: unknown, after: unknown): unknown | typeof NO_CHANGE => {
  if (Object.is(before, after)) {
    return NO_CHANGE
  }

  if (Array.isArray(before) || Array.isArray(after)) {
    return JSON.stringify(before) === JSON.stringify(after) ? NO_CHANGE : after
  }

  if (isPlainObject(before) && isPlainObject(after)) {
    const patch: JsonObject = {}

    for (const [key, afterValue] of Object.entries(after)) {
      const diff = buildPatch((before as JsonObject)[key], afterValue)
      if (diff !== NO_CHANGE) {
        patch[key] = diff
      }
    }

    return Object.keys(patch).length > 0 ? patch : NO_CHANGE
  }

  return after
}

export const useSettingsStore = defineStore('settings', () => {
  const appSettings = ref<AppSettings>(DEFAULT_APP_SETTINGS)
  const isLoading = ref(false)
  const error = ref<string | null>(null)
  const isInitialized = ref(false)
  const { setLocale } = useI18n()
  let settingsChangedHandler: ((params: unknown) => void) | null = null
  let refreshTimer: ReturnType<typeof setTimeout> | null = null
  let refreshInFlight: Promise<void> | null = null

  const refreshFromBackend = async () => {
    if (refreshInFlight) {
      await refreshInFlight
      return
    }

    refreshInFlight = (async () => {
      try {
        const settings = await settingsApi.get()
        appSettings.value = settings
        error.value = null
      } catch (e) {
        error.value = (e as Error).message
      } finally {
        refreshInFlight = null
      }
    })()

    await refreshInFlight
  }

  const scheduleRefreshFromBackend = () => {
    if (refreshTimer) {
      return
    }

    refreshTimer = setTimeout(() => {
      refreshTimer = null
      void refreshFromBackend()
    }, 120)
  }

  const subscribeSettingsChanged = () => {
    if (settingsChangedHandler) {
      return
    }

    settingsChangedHandler = () => {
      scheduleRefreshFromBackend()
    }
    on('settings.changed', settingsChangedHandler)
  }

  const unsubscribeSettingsChanged = () => {
    if (settingsChangedHandler) {
      off('settings.changed', settingsChangedHandler)
      settingsChangedHandler = null
    }
    if (refreshTimer) {
      clearTimeout(refreshTimer)
      refreshTimer = null
    }
  }

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
      subscribeSettingsChanged()
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
    const oldSettings = cloneDeep(appSettings.value)
    const nextSettings = deepMerge(oldSettings, newSettings) as AppSettings
    const patch = buildPatch(oldSettings, nextSettings)

    if (patch === NO_CHANGE) {
      return
    }

    appSettings.value = nextSettings

    try {
      await settingsApi.patch(patch as Partial<AppSettings>)
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

  const dispose = () => {
    unsubscribeSettingsChanged()
  }

  return {
    appSettings,
    isLoading,
    error,
    isInitialized,
    init,
    updateSettings,
    clearError,
    dispose,
  }
})
