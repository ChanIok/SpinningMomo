import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import { storeToRefs } from 'pinia'

type UpdateSourceMode = 'cn_first' | 'github_first'

const DOWNLOAD_SOURCES = {
  github: {
    name: 'GitHub',
    urlTemplate: 'https://github.com/ChanIok/SpinningMomo/releases/download/v{0}/{1}',
  },
  mirror: {
    name: 'Mirror',
    urlTemplate: 'https://r2.infinitymomo.com/releases/v{0}/{1}',
  },
  cnb: {
    name: 'CNB',
    urlTemplate: 'https://cnb.cool/infinitymomo/SpinningMomo/-/releases/download/v{0}/{1}',
  },
} as const

export const useGeneralActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateLanguage = async (language: string) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        language: { current: language },
      },
    })
  }

  const updateLoggerLevel = async (level: string) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        logger: { level },
      },
    })
  }

  const updateAutoCheck = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      update: {
        ...appSettings.value.update,
        autoCheck: enabled,
        autoUpdateOnExit: enabled ? appSettings.value.update.autoUpdateOnExit : false,
      },
    })
  }

  const updateAutoUpdateOnExit = async (enabled: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      update: {
        ...appSettings.value.update,
        autoUpdateOnExit: enabled,
      },
    })
  }

  const updateDownloadSourceMode = async (mode: UpdateSourceMode) => {
    const downloadSources =
      mode === 'cn_first'
        ? [DOWNLOAD_SOURCES.cnb, DOWNLOAD_SOURCES.mirror, DOWNLOAD_SOURCES.github]
        : [DOWNLOAD_SOURCES.github, DOWNLOAD_SOURCES.mirror, DOWNLOAD_SOURCES.cnb]

    await store.updateSettings({
      ...appSettings.value,
      update: {
        ...appSettings.value.update,
        downloadSources,
      },
    })
  }

  const updateFloatingWindowHotkey = async (modifiers: number, key: number) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        hotkey: {
          ...appSettings.value.app.hotkey,
          floatingWindow: { modifiers, key },
        },
      },
    })
  }

  const updateScreenshotHotkey = async (modifiers: number, key: number) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        hotkey: {
          ...appSettings.value.app.hotkey,
          screenshot: { modifiers, key },
        },
      },
    })
  }

  const updateRecordingHotkey = async (modifiers: number, key: number) => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        hotkey: {
          ...appSettings.value.app.hotkey,
          recording: { modifiers, key },
        },
      },
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
          floatingWindow: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.floatingWindow.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.floatingWindow.key,
          },
          screenshot: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.screenshot.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.screenshot.key,
          },
          recording: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.recording.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.recording.key,
          },
        },
      },
      update: {
        ...appSettings.value.update,
        autoCheck: DEFAULT_APP_SETTINGS.update.autoCheck,
        autoUpdateOnExit: DEFAULT_APP_SETTINGS.update.autoUpdateOnExit,
      },
    })
  }

  const resetGeneralCoreSettings = async () => {
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
      },
      update: {
        ...appSettings.value.update,
        autoCheck: DEFAULT_APP_SETTINGS.update.autoCheck,
        autoUpdateOnExit: DEFAULT_APP_SETTINGS.update.autoUpdateOnExit,
      },
    })
  }

  const resetHotkeySettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      app: {
        ...appSettings.value.app,
        hotkey: {
          floatingWindow: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.floatingWindow.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.floatingWindow.key,
          },
          screenshot: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.screenshot.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.screenshot.key,
          },
          recording: {
            modifiers: DEFAULT_APP_SETTINGS.app.hotkey.recording.modifiers,
            key: DEFAULT_APP_SETTINGS.app.hotkey.recording.key,
          },
        },
      },
    })
  }

  return {
    updateLanguage,
    updateLoggerLevel,
    updateAutoCheck,
    updateAutoUpdateOnExit,
    updateDownloadSourceMode,
    updateFloatingWindowHotkey,
    updateScreenshotHotkey,
    updateRecordingHotkey,
    resetGeneralSettings,
    resetGeneralCoreSettings,
    resetHotkeySettings,
  }
}
