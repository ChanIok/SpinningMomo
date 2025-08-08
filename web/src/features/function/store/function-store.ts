import { useSettingsStore } from '@/lib/settings'

// 重新导出共享的store钩子
export const useFunctionStore = useSettingsStore

// 扩展function特有的业务方法（目前与menu共用相同的store）
export const useFunctionActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()
  
  // 乐观更新：更新窗口标题
  const updateWindowTitle = async (title: string) => {
    await updateSettings({
      window: {
        ...appSettings.window,
        targetTitle: title
      }
    })
  }
  
  // 乐观更新：更新截图目录
  const updateScreenshotDir = async (dirPath: string) => {
    await updateSettings({
      features: {
        ...appSettings.features,
        screenshot: {
          ...appSettings.features.screenshot,
          screenshotDirPath: dirPath
        }
      }
    })
  }
  
  return {
    appSettings,
    updateWindowTitle,
    updateScreenshotDir
  }
}