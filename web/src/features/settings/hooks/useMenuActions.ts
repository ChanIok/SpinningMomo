import { useSettingsStore } from '@/lib/settings'
import type { FeatureItem, PresetItem } from '@/lib/settings/settingsTypes'

// 扩展menu特有的业务方法
export const useMenuActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()

  // 乐观更新：更新功能项
  const updateFeatureItems = async (items: FeatureItem[]) => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appMenu: {
          ...appSettings.ui.appMenu,
          featureItems: items,
        },
      },
    })
  }

  // 乐观更新：更新比例设置
  const updateAspectRatios = async (items: PresetItem[]) => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appMenu: {
          ...appSettings.ui.appMenu,
          aspectRatios: items,
        },
      },
    })
  }

  // 乐观更新：更新分辨率设置
  const updateResolutions = async (items: PresetItem[]) => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appMenu: {
          ...appSettings.ui.appMenu,
          resolutions: items,
        },
      },
    })
  }

  return {
    appSettings,
    updateFeatureItems,
    updateAspectRatios,
    updateResolutions,
  }
}
