import { useSettingsStore } from '@/lib/settings'
import { DEFAULT_APP_SETTINGS } from '@/lib/settings/settingsTypes'

// 扩展function特有的业务方法
export const useFunctionActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()

  // 乐观更新：更新窗口标题
  const updateWindowTitle = async (title: string) => {
    await updateSettings({
      window: {
        ...appSettings.window,
        targetTitle: title,
      },
    })
  }

  // 乐观更新：更新截图目录
  const updateScreenshotDir = async (dirPath: string) => {
    await updateSettings({
      features: {
        ...appSettings.features,
        screenshot: {
          ...appSettings.features.screenshot,
          screenshotDirPath: dirPath,
        },
      },
    })
  }

  // 乐观更新：切换 调整时置底任务栏
  const updateTaskbarLowerOnResize = async (enabled: boolean) => {
    await updateSettings({
      window: {
        ...appSettings.window,
        taskbar: {
          ...appSettings.window.taskbar,
          lowerOnResize: enabled,
        },
      },
    })
  }

  // 乐观更新：切换 黑边模式
  const updateLetterboxEnabled = async (enabled: boolean) => {
    await updateSettings({
      features: {
        ...appSettings.features,
        letterbox: {
          ...appSettings.features.letterbox,
          enabled,
        },
      },
    })
  }

  // 重置功能设置为默认值
  const resetFunctionSettings = async () => {
    await updateSettings({
      window: {
        ...appSettings.window,
        targetTitle: DEFAULT_APP_SETTINGS.window.targetTitle,
        taskbar: {
          ...appSettings.window.taskbar,
          lowerOnResize: DEFAULT_APP_SETTINGS.window.taskbar.lowerOnResize,
        },
      },
      features: {
        ...appSettings.features,
        screenshot: {
          ...appSettings.features.screenshot,
          screenshotDirPath: DEFAULT_APP_SETTINGS.features.screenshot.screenshotDirPath,
        },
        letterbox: {
          ...appSettings.features.letterbox,
          enabled: DEFAULT_APP_SETTINGS.features.letterbox.enabled,
        },
      },
    })
  }

  return {
    appSettings,
    updateWindowTitle,
    updateScreenshotDir,
    updateTaskbarLowerOnResize,
    updateLetterboxEnabled,
    resetFunctionSettings,
  }
}
