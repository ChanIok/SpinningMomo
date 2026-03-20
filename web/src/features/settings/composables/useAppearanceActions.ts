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
import {
  selectBackgroundImage,
  copyBackgroundImageToResources,
  removeBackgroundImageResource,
  analyzeBackground,
} from '../api'
import type { OverlayPalette, OverlayPalettePreset } from '../overlayPalette'
import { getOverlayPaletteFromBackground, toBackgroundOverlayPatch } from '../overlayPalette'
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
        webviewWindow: {
          ...appSettings.value.ui.webviewWindow,
          enableTransparentBackground:
            DEFAULT_APP_SETTINGS.ui.webviewWindow.enableTransparentBackground,
        },
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
        webviewWindow: {
          ...appSettings.value.ui.webviewWindow,
          enableTransparentBackground:
            DEFAULT_APP_SETTINGS.ui.webviewWindow.enableTransparentBackground,
        },
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

  const updateSurfaceOpacity = async (surfaceOpacity: number) => {
    await updateBackgroundSettings({ surfaceOpacity })
  }

  const updateBackgroundOpacity = async (backgroundOpacity: number) => {
    await updateBackgroundSettings({ backgroundOpacity })
  }

  const updateBackgroundBlur = async (backgroundBlurAmount: number) => {
    await updateBackgroundSettings({ backgroundBlurAmount })
  }

  const updateOverlayOpacity = async (overlayOpacity: number) => {
    await updateBackgroundSettings({ overlayOpacity })
  }

  const updatePrimaryColor = async (primaryColor: string) => {
    await updateBackgroundSettings({ primaryColor })
  }

  const updateOverlayPalette = async (palette: OverlayPalette) => {
    await updateBackgroundSettings(toBackgroundOverlayPatch(palette))
  }

  const applyOverlayPalettePreset = async (preset: OverlayPalettePreset) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        webTheme: {
          ...appSettings.value.ui.webTheme,
          mode: preset.themeMode,
        },
        background: {
          ...appSettings.value.ui.background,
          ...toBackgroundOverlayPatch(preset),
        },
      },
    })
  }

  const updateWebViewTransparentBackground = async (enableTransparentBackground: boolean) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        webviewWindow: {
          ...appSettings.value.ui.webviewWindow,
          enableTransparentBackground,
        },
      },
    })
  }

  const updateCustomCss = async (customCss: string) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        webTheme: {
          ...appSettings.value.ui.webTheme,
          customCss,
        },
      },
    })
  }

  const applyWallpaperAnalysis = async (imagePath: string, persistImage = false) => {
    const currentBackground = appSettings.value.ui.background
    const overlayMode = getOverlayPaletteFromBackground(currentBackground).mode
    const analysis = await analyzeBackground(imagePath, overlayMode)

    const nextBackground = {
      ...currentBackground,
      ...(persistImage ? { type: 'image' as const, imagePath } : {}),
      overlayColors: analysis.overlayColors.slice(0, overlayMode),
      primaryColor: analysis.primaryColor,
    }

    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        webTheme: {
          ...appSettings.value.ui.webTheme,
          mode: analysis.themeMode,
        },
        background: nextBackground,
      },
    })
  }

  const handleBackgroundImageSelect = async () => {
    try {
      const previousImagePath = appSettings.value.ui.background.imagePath
      const imagePath = await selectBackgroundImage()
      if (imagePath) {
        const copiedImagePath = await copyBackgroundImageToResources(imagePath)
        try {
          await applyWallpaperAnalysis(copiedImagePath, true)
        } catch (error) {
          console.warn('分析背景图片失败，使用基础背景设置:', error)
          await updateBackgroundSettings({
            type: 'image',
            imagePath: copiedImagePath,
          })
        }
        if (previousImagePath && previousImagePath !== copiedImagePath) {
          void removeBackgroundImageResource(previousImagePath)
        }
      }
    } catch (error) {
      console.error('设置背景图片失败:', error)
      throw error
    }
  }

  const handleBackgroundImageRemove = async () => {
    try {
      const previousImagePath = appSettings.value.ui.background.imagePath
      await updateBackgroundSettings({
        type: 'none',
        imagePath: '',
      })
      if (previousImagePath) {
        void removeBackgroundImageResource(previousImagePath)
      }
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
    updateOverlayOpacity,
    updatePrimaryColor,
    updateOverlayPalette,
    applyOverlayPalettePreset,
    updateWebViewTransparentBackground,
    updateCustomCss,
    updateSurfaceOpacity,
    handleBackgroundImageSelect,
    handleBackgroundImageRemove,
    applyWallpaperAnalysis,
    getFloatingWindowColorsByTheme,
    updateFloatingWindowTheme,
  }
}
