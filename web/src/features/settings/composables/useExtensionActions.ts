import { storeToRefs } from 'pinia'
import { useSettingsStore } from '../store'
import type { AppSettings } from '../types'

const DEFAULT_INFINITY_NIKKI_SETTINGS: AppSettings['plugins']['infinityNikki'] = {
  enable: false,
  gameDir: '',
  galleryGuideSeen: false,
  allowOnlinePhotoMetadataExtract: false,
  manageScreenshotHardlinks: false,
}

export const useExtensionActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateInfinityNikkiSettings = async (
    patch: Partial<AppSettings['plugins']['infinityNikki']>
  ) => {
    await store.updateSettings({
      plugins: {
        infinityNikki: {
          ...appSettings.value.plugins.infinityNikki,
          ...patch,
        },
      },
    })
  }

  const updateInfinityNikkiEnabled = async (enable: boolean) => {
    await updateInfinityNikkiSettings({ enable })
  }

  const updateInfinityNikkiGameDir = async (gameDir: string) => {
    await updateInfinityNikkiSettings({ gameDir })
  }

  const updateInfinityNikkiAllowOnlinePhotoMetadataExtract = async (enabled: boolean) => {
    await updateInfinityNikkiSettings({ allowOnlinePhotoMetadataExtract: enabled })
  }

  const updateInfinityNikkiManageScreenshotHardlinks = async (enabled: boolean) => {
    await updateInfinityNikkiSettings({ manageScreenshotHardlinks: enabled })
  }

  const completeInfinityNikkiInitialization = async () => {
    await updateInfinityNikkiSettings({ galleryGuideSeen: true })
  }

  const resetExtensionSettings = async () => {
    await store.updateSettings({
      plugins: {
        infinityNikki: {
          ...DEFAULT_INFINITY_NIKKI_SETTINGS,
        },
      },
    })
  }

  return {
    updateInfinityNikkiEnabled,
    updateInfinityNikkiGameDir,
    updateInfinityNikkiAllowOnlinePhotoMetadataExtract,
    updateInfinityNikkiManageScreenshotHardlinks,
    completeInfinityNikkiInitialization,
    resetExtensionSettings,
  }
}
