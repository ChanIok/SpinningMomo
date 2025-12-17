
import { useSettingsStore } from '../store'
import { useWebSettingsStore } from '@/features/web-settings/store'
import { DEFAULT_APP_SETTINGS, DARK_APP_WINDOW_COLORS, LIGHT_APP_WINDOW_COLORS } from '../types'
import type { AppWindowLayout, AppWindowColors, AppWindowThemeMode } from '../types'
import type { ThemeSettings, WebSettings } from '@/features/web-settings/types'
import { selectBackgroundImage, copyBackgroundImageToResources } from '@/features/web-settings/api'
import { storeToRefs } from 'pinia'

export const useAppearanceActions = () => {
    const store = useSettingsStore()
    const { appSettings } = storeToRefs(store)
    const webSettingsStore = useWebSettingsStore()
    const { webSettings } = storeToRefs(webSettingsStore)
    const { loadSettings, updateSettings: updateWebSettings } = webSettingsStore

    const updateAppWindowLayout = async (layout: AppWindowLayout) => {
        await store.updateSettings({
            ...appSettings.value,
            ui: {
                ...appSettings.value.ui,
                appWindowLayout: layout
            }
        })
    }

    const resetAppearanceSettings = async () => {
        await store.updateSettings({
            ...appSettings.value,
            ui: {
                ...appSettings.value.ui,
                appWindowLayout: DEFAULT_APP_SETTINGS.ui.appWindowLayout,
                appWindowColors: DEFAULT_APP_SETTINGS.ui.appWindowColors,
                appWindowThemeMode: DEFAULT_APP_SETTINGS.ui.appWindowThemeMode,
            }
        })
    }

    const getAppWindowColorsByTheme = (themeMode: AppWindowThemeMode): AppWindowColors => {
        switch (themeMode) {
            case 'light': return LIGHT_APP_WINDOW_COLORS
            case 'dark': return DARK_APP_WINDOW_COLORS
            default: return DARK_APP_WINDOW_COLORS
        }
    }

    const updateAppWindowTheme = async (themeMode: AppWindowThemeMode) => {
        const colors = getAppWindowColorsByTheme(themeMode)
        await store.updateSettings({
            ...appSettings.value,
            ui: {
                ...appSettings.value.ui,
                appWindowThemeMode: themeMode,
                appWindowColors: colors
            }
        })
    }

    const updateAppWindowColors = async (colors: AppWindowColors) => {
        await store.updateSettings({
            ...appSettings.value,
            ui: {
                ...appSettings.value.ui,
                appWindowColors: colors
            }
        })
    }

    const updateBackgroundSettings = async (partialBackground: Partial<WebSettings['ui']['background']>) => {
        await updateWebSettings({
            ui: {
                ...webSettings.value.ui,
                background: {
                    ...webSettings.value.ui.background,
                    ...partialBackground
                }
            }
        })
    }

    const updateThemeSettings = async (partialTheme: Partial<ThemeSettings>) => {
        await updateWebSettings({
            ui: {
                ...webSettings.value.ui,
                theme: {
                    ...webSettings.value.ui.theme,
                    ...partialTheme
                }
            }
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
                    imagePath: copiedImagePath
                })
                await loadSettings()
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
                imagePath: ''
            })
        } catch (error) {
           console.error('移除背景图片失败:', error)
           throw error
        }
    }

    return {
        updateAppWindowLayout,
        resetAppearanceSettings,
        updateAppWindowColors,
        updateBackgroundOpacity,
        updateBackgroundBlur,
        handleBackgroundImageSelect,
        handleBackgroundImageRemove,
        updateThemeSettings,
        getAppWindowColorsByTheme,
        updateAppWindowTheme
    }

}
