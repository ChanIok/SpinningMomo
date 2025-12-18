
import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import type { MenuItem } from '../types'
import { storeToRefs } from 'pinia'

export const useMenuActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateFeatureItems = async (items: MenuItem[]) => {
    // 提取启用的 ID 列表（保持顺序）
    const enabledIds = items.filter(item => item.enabled).map(item => item.id)
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          enabledFeatures: enabledIds,
        },
      },
    })
  }

  const updateAspectRatios = async (items: MenuItem[]) => {
    const enabledIds = items.filter(item => item.enabled).map(item => item.id)
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          aspectRatios: enabledIds,
        },
      },
    })
  }

  const updateResolutions = async (items: MenuItem[]) => {
    const enabledIds = items.filter(item => item.enabled).map(item => item.id)
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          resolutions: enabledIds,
        },
      },
    })
  }

  const resetMenuSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          enabledFeatures: DEFAULT_APP_SETTINGS.ui.appMenu.enabledFeatures,
          aspectRatios: DEFAULT_APP_SETTINGS.ui.appMenu.aspectRatios,
          resolutions: DEFAULT_APP_SETTINGS.ui.appMenu.resolutions,
        },
      },
    })
  }

  return {
    updateFeatureItems,
    updateAspectRatios,
    updateResolutions,
    resetMenuSettings,
  }
}
