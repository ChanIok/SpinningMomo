import { useSettingsStore } from '@/lib/settings'
import type { AppWindowLayout } from './types'

// 重新导出共享的store钩子
export const useLayoutStore = useSettingsStore

// 扩展layout特有的业务方法
export const useLayoutActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()
  
  // 乐观更新：更新AppWindow布局设置
  const updateAppWindowLayout = async (layout: AppWindowLayout) => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appWindowLayout: layout
      }
    })
  }
  
  return {
    appSettings,
    updateAppWindowLayout
  }
}