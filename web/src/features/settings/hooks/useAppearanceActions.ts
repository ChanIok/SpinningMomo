import { useSettingsStore } from '@/lib/settings'
import type { AppWindowLayout } from '@/lib/settings/settingsTypes'

// 扩展appearance特有的业务方法
export const useAppearanceActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()

  // 乐观更新：更新AppWindow布局设置
  const updateAppWindowLayout = async (layout: AppWindowLayout) => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appWindowLayout: layout,
      },
    })
  }

  return {
    appSettings,
    updateAppWindowLayout,
  }
}
