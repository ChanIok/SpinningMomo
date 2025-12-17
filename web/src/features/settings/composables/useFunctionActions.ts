
import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import { storeToRefs } from 'pinia'

export const useFunctionActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateWindowTitle = async (title: string) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        targetTitle: title,
      },
    })
  }

  const updateScreenshotDir = async (dirPath: string) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...appSettings.value.features.screenshot,
          screenshotDirPath: dirPath,
        },
      },
    })
  }

  const updateTaskbarLowerOnResize = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        taskbar: {
          ...appSettings.value.window.taskbar,
          lowerOnResize: enabled,
        },
      },
    })
  }

  const updateLetterboxEnabled = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      features: {
        ...appSettings.value.features,
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled,
        },
      },
    })
  }

  const resetFunctionSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      window: {
        ...appSettings.value.window,
        targetTitle: DEFAULT_APP_SETTINGS.window.targetTitle,
        taskbar: {
          ...appSettings.value.window.taskbar,
          lowerOnResize: DEFAULT_APP_SETTINGS.window.taskbar.lowerOnResize,
        },
      },
      features: {
        ...appSettings.value.features,
        screenshot: {
          ...appSettings.value.features.screenshot,
          screenshotDirPath: DEFAULT_APP_SETTINGS.features.screenshot.screenshotDirPath,
        },
        letterbox: {
          ...appSettings.value.features.letterbox,
          enabled: DEFAULT_APP_SETTINGS.features.letterbox.enabled,
        },
      },
    })
  }

  return {
    updateWindowTitle,
    updateScreenshotDir,
    updateTaskbarLowerOnResize,
    updateLetterboxEnabled,
    resetFunctionSettings
  }
}
