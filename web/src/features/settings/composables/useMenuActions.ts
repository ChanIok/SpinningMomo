
import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import type { FeatureItem, PresetItem } from '../types'
import { storeToRefs } from 'pinia'

export const useMenuActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateFeatureItems = async (items: FeatureItem[]) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          featureItems: items,
        },
      },
    })
  }

  const updateAspectRatios = async (items: PresetItem[]) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          aspectRatios: items,
        },
      },
    })
  }

  const updateResolutions = async (items: PresetItem[]) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          resolutions: items,
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
          featureItems: DEFAULT_APP_SETTINGS.ui.appMenu.featureItems,
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
