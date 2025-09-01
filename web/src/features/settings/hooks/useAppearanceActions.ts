import { useSettingsStore } from '@/lib/settings'
import { useWebSettingsStore } from '@/lib/web-settings'
import {
  DEFAULT_APP_SETTINGS,
  DARK_APP_WINDOW_COLORS,
  LIGHT_APP_WINDOW_COLORS,
} from '@/lib/settings/settingsTypes'
import type {
  AppWindowLayout,
  AppWindowColors,
  AppWindowThemeMode,
} from '@/lib/settings/settingsTypes'
import type { ThemeSettings, WebSettings } from '@/lib/web-settings/webSettingsTypes'
import {
  writeWebSettings,
  selectBackgroundImage,
  copyBackgroundImageToResources,
} from '@/lib/web-settings/webSettingsApi'

// 扩展appearance特有的业务方法
export const useAppearanceActions = () => {
  const { appSettings, updateSettings } = useSettingsStore()
  const { webSettings, setSettings, setError, loadSettings } = useWebSettingsStore()

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
        appWindowColors: DEFAULT_APP_SETTINGS.ui.appWindowColors,
        appWindowThemeMode: DEFAULT_APP_SETTINGS.ui.appWindowThemeMode,
      },
    })
  }

  // 根据主题模式获取对应的AppWindow颜色
  const getAppWindowColorsByTheme = (themeMode: AppWindowThemeMode): AppWindowColors => {
    switch (themeMode) {
      case 'light':
        return LIGHT_APP_WINDOW_COLORS
      case 'dark':
        return DARK_APP_WINDOW_COLORS
      default:
        return DARK_APP_WINDOW_COLORS
    }
  }

  // 更新AppWindow主题（包括主题模式和颜色）
  const updateAppWindowTheme = async (themeMode: AppWindowThemeMode) => {
    const colors = getAppWindowColorsByTheme(themeMode)

    // 同时更新主题模式和颜色
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appWindowThemeMode: themeMode,
        appWindowColors: colors,
      },
    })
  }

  // 更新AppWindow颜色设置
  const updateAppWindowColors = async (colors: AppWindowColors) => {
    await updateSettings({
      ui: {
        ...appSettings.ui,
        appWindowColors: colors,
      },
    })
  }

  // 更新背景设置（乐观更新）
  const updateBackgroundSettings = async (
    partialBackground: Partial<WebSettings['ui']['background']>
  ) => {
    const previousSettings = webSettings

    // 1. 立即更新本地状态（乐观更新）
    const optimisticSettings = {
      ...webSettings,
      ui: {
        ...webSettings.ui,
        background: {
          ...webSettings.ui.background,
          ...partialBackground,
        },
      },
    }

    setSettings(optimisticSettings)

    try {
      // 2. 同步到文件
      await writeWebSettings(optimisticSettings)
      console.log('✅ 背景设置已更新:', partialBackground)
    } catch (error) {
      // 3. 失败时回滚
      setSettings(previousSettings)
      setError(error instanceof Error ? error.message : '更新背景设置失败')
      console.error('❌ 背景设置更新失败，已回滚:', error)
      throw error
    }
  }

  // 更新主题设置（乐观更新）
  const updateThemeSettings = async (partialTheme: Partial<ThemeSettings>) => {
    const previousSettings = webSettings

    // 1. 立即更新本地状态（乐观更新）
    const optimisticSettings = {
      ...webSettings,
      ui: {
        ...webSettings.ui,
        theme: {
          ...webSettings.ui.theme,
          ...partialTheme,
        },
      },
    }

    setSettings(optimisticSettings)

    try {
      // 2. 同步到文件
      await writeWebSettings(optimisticSettings)
      console.log('✅ 主题设置已更新:', partialTheme)
    } catch (error) {
      // 3. 失败时回滚
      setSettings(previousSettings)
      setError(error instanceof Error ? error.message : '更新主题设置失败')
      console.error('❌ 主题设置更新失败，已回滚:', error)
      throw error
    }
  }

  // 更新背景不透明度
  const updateBackgroundOpacity = async (opacity: number) => {
    await updateBackgroundSettings({ opacity })
  }

  // 更新背景模糊度
  const updateBackgroundBlur = async (blurAmount: number) => {
    await updateBackgroundSettings({ blurAmount })
  }

  // 选择并设置背景图片
  const handleBackgroundImageSelect = async () => {
    try {
      const imagePath = await selectBackgroundImage()
      if (imagePath) {
        // 复制图片到资源目录
        const copiedImagePath = await copyBackgroundImageToResources(imagePath)

        // 使用复制后的路径更新设置
        await updateBackgroundSettings({
          type: 'image',
          imagePath: copiedImagePath,
        })

        // 重新读取state，刷新背景图片
        await loadSettings()
      }
    } catch (error) {
      console.error('设置背景图片失败:', error)
      throw error
    }
  }

  // 移除背景图片
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
    updateAppWindowLayout,
    resetAppearanceSettings,
    updateAppWindowColors,
    updateBackgroundOpacity,
    updateBackgroundBlur,
    handleBackgroundImageSelect,
    handleBackgroundImageRemove,
    updateThemeSettings,
    getAppWindowColorsByTheme,
    updateAppWindowTheme,
  }
}
