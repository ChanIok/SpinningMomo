import { useSettingsStore } from '@/lib/settings'
import { DEFAULT_APP_SETTINGS } from '@/lib/settings/settingsTypes'
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

  // 重置外观设置为默认值
  const resetAppearanceSettings = async () => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appWindowLayout: DEFAULT_APP_SETTINGS.ui.appWindowLayout,
      },
    })
  }

  return {
    appSettings,
    updateAppWindowLayout,
    resetAppearanceSettings,
  }
}
