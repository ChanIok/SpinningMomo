import { useSettingsStore } from '../store'
import {
  DEFAULT_APP_SETTINGS,
  DARK_FLOATING_WINDOW_COLORS,
  LIGHT_FLOATING_WINDOW_COLORS,
} from '../types'
import type {
  FloatingWindowLayout,
  FloatingWindowColors,
  FloatingWindowThemeMode,
  WebBackgroundSettings,
} from '../types'
import { selectBackgroundImage, copyBackgroundImageToResources } from '../api'
import { storeToRefs } from 'pinia'

export const useAppearanceActions = () => {
  const store = useSettingsStore()
  const { appSettings } = storeToRefs(store)

  const updateFloatingWindowLayout = async (layout: FloatingWindowLayout) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        floatingWindowLayout: layout,
      },
    })
  }

  const resetAppearanceSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        floatingWindowLayout: DEFAULT_APP_SETTINGS.ui.floatingWindowLayout,
        floatingWindowColors: DEFAULT_APP_SETTINGS.ui.floatingWindowColors,
        floatingWindowThemeMode: DEFAULT_APP_SETTINGS.ui.floatingWindowThemeMode,
        webTheme: DEFAULT_APP_SETTINGS.ui.webTheme,
        background: DEFAULT_APP_SETTINGS.ui.background,
      },
    })
  }

  const resetWebAppearanceSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        webTheme: DEFAULT_APP_SETTINGS.ui.webTheme,
        background: DEFAULT_APP_SETTINGS.ui.background,
      },
    })
  }

  const resetFloatingWindowSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        floatingWindowLayout: DEFAULT_APP_SETTINGS.ui.floatingWindowLayout,
        floatingWindowColors: DEFAULT_APP_SETTINGS.ui.floatingWindowColors,
        floatingWindowThemeMode: DEFAULT_APP_SETTINGS.ui.floatingWindowThemeMode,
      },
    })
  }

  const getFloatingWindowColorsByTheme = (
    themeMode: FloatingWindowThemeMode
  ): FloatingWindowColors => {
    switch (themeMode) {
      case 'light':
        return LIGHT_FLOATING_WINDOW_COLORS
      case 'dark':
        return DARK_FLOATING_WINDOW_COLORS
      default:
        return DARK_FLOATING_WINDOW_COLORS
    }
  }

  const updateFloatingWindowTheme = async (themeMode: FloatingWindowThemeMode) => {
    const colors = getFloatingWindowColorsByTheme(themeMode)
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        floatingWindowThemeMode: themeMode,
        floatingWindowColors: colors,
      },
    })
  }

  const updateFloatingWindowColors = async (colors: FloatingWindowColors) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        floatingWindowColors: colors,
      },
    })
  }

  const updateBackgroundSettings = async (partialBackground: Partial<WebBackgroundSettings>) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        background: {
          ...appSettings.value.ui.background,
          ...partialBackground,
        },
      },
    })
  }

  const updateBackgroundOpacity = async (opacity: number) => {
    await updateBackgroundSettings({ opacity })
  }

  const updateBackgroundBlur = async (blurAmount: number) => {
    await updateBackgroundSettings({ blurAmount })
  }

  const handleBackgroundImageSelect = async () => {
    try {
      const imagePath = await selectBackgroundImage()
      if (imagePath) {
        const copiedImagePath = await copyBackgroundImageToResources(imagePath)
        await updateBackgroundSettings({
          type: 'image',
          imagePath: copiedImagePath,
        })
      }
    } catch (error) {
      console.error('设置背景图片失败:', error)
      throw error
    }
  }

  const handleBackgroundImageRemove = async () => {
    try {
      await updateBackgroundSettings({
        type: 'none',
        imagePath: '',
      })
    } catch (error) {
      console.error('移除背景图片失败:', error)
      throw error
    }
  }

  return {
    updateFloatingWindowLayout,
    resetAppearanceSettings,
    resetWebAppearanceSettings,
    resetFloatingWindowSettings,
    updateFloatingWindowColors,
    updateBackgroundOpacity,
    updateBackgroundBlur,
    handleBackgroundImageSelect,
    handleBackgroundImageRemove,
    getFloatingWindowColorsByTheme,
    updateFloatingWindowTheme,
  }
}
