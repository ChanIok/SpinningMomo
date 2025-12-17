
import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import { storeToRefs } from 'pinia'

export const useGeneralActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateLanguage = async (language: string) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        language: { current: language }
      }
    })
  }

  const updateLoggerLevel = async (level: string) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        logger: { level }
      }
    })
  }

  const updateToggleVisibilityHotkey = async (modifiers: number, key: number) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        hotkey: {
          ...appSettings.value.app.hotkey,
          toggleVisibility: { modifiers, key }
        }
      }
    })
  }

  const updateScreenshotHotkey = async (modifiers: number, key: number) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        hotkey: {
          ...appSettings.value.app.hotkey,
          screenshot: { modifiers, key }
        }
      }
    })
  }

  const resetGeneralSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        language: {
          current: DEFAULT_APP_SETTINGS.app.language.current,
        },
        logger: {
          level: DEFAULT_APP_SETTINGS.app.logger.level,
        },
        hotkey: {
          toggleVisibility: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.toggleVisibility.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.toggleVisibility.key,
          },
          screenshot: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.screenshot.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.screenshot.key,
          },
        },
      },
    })
  }

  return {
    updateLanguage,
    updateLoggerLevel,
    updateToggleVisibilityHotkey,
    updateScreenshotHotkey,
    resetGeneralSettings
  }
}
