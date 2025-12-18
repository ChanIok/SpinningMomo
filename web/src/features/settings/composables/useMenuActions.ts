import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import type { MenuItem } from '../types'
import { storeToRefs } from 'pinia'

export const useMenuActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateFeatureItems = async (items: MenuItem[]) => {
    // 直接存储完整的 items 数组（包含顺序和启用状态）
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          features: items,
        },
      },
    })
  }

  const updateAspectRatios = async (items: MenuItem[]) => {
    const enabledIds = items.filter((item) => item.enabled).map((item) => item.id)
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
    const enabledIds = items.filter((item) => item.enabled).map((item) => item.id)
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
          features: DEFAULT_APP_SETTINGS.ui.appMenu.features,
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
