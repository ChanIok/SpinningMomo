import { useSettingsStore } from '@/lib/settings'
import { DEFAULT_APP_SETTINGS } from '@/lib/settings/settingsTypes'

// 扩展general特有的业务方法
export const useGeneralActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()

  // 乐观更新：更新语言设置
  const updateLanguage = async (language: string) => {
    await updateSettings({
      app: {
        ...appSettings.app,
        language: { current: language },
      },
    })
  }

  // 乐观更新：更新日志级别
  const updateLoggerLevel = async (level: string) => {
    await updateSettings({
      app: {
        ...appSettings.app,
        logger: { level },
      },
    })
  }

  // 乐观更新：更新浮窗显示/隐藏快捷键
  const updateToggleVisibilityHotkey = async (modifiers: number, key: number) => {
    await updateSettings({
      app: {
        ...appSettings.app,
        hotkey: {
          ...appSettings.app.hotkey,
          toggleVisibility: { modifiers, key },
        },
      },
    })
  }

  // 乐观更新：更新截图快捷键
  const updateScreenshotHotkey = async (modifiers: number, key: number) => {
    await updateSettings({
      app: {
        ...appSettings.app,
        hotkey: {
          ...appSettings.app.hotkey,
          screenshot: { modifiers, key },
        },
      },
    })
  }

  // 重置通用设置为默认值
  const resetGeneralSettings = async () => {
    await updateSettings({
      app: {
        ...appSettings.app,
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
    appSettings,
    updateLanguage,
    updateLoggerLevel,
    updateToggleVisibilityHotkey,
    updateScreenshotHotkey,
    resetGeneralSettings,
  }
}
