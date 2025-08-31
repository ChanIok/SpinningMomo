import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type {
  WebSettings,
  WebBackgroundSettings,
  ThemeSettings,
  WebSettingsState,
} from './webSettingsTypes'
import { DEFAULT_WEB_SETTINGS } from './webSettingsTypes'
import {
  readWebSettings,
  writeWebSettings,
  initializeWebSettings,
  selectBackgroundImage,
  copyBackgroundImageToResources,
} from './webSettingsApi'

interface WebSettingsActions {
  // 基础操作
  setSettings: (settings: WebSettings) => void
  setError: (error: string | null) => void
  clearError: () => void
  setIsInitialized: (initialized: boolean) => void

  // 业务操作
  initialize: () => Promise<void>
  updateBackgroundSettings: (background: Partial<WebBackgroundSettings>) => Promise<void>
  updateThemeSettings: (theme: Partial<ThemeSettings>) => Promise<void>
  selectAndSetBackgroundImage: () => Promise<void>
  removeBackgroundImage: () => Promise<void>
  loadSettings: () => Promise<void>
  resetToDefault: () => Promise<void>

  // 清理
  cleanup: () => void
}

type WebSettingsStoreType = WebSettingsState & WebSettingsActions

export const useWebSettingsStore = create<WebSettingsStoreType>()(
  devtools(
    (set, get) => ({
      // 初始状态
      settings: DEFAULT_WEB_SETTINGS,
      error: null,
      isInitialized: false,

      // 基础操作
      setSettings: (settings: WebSettings) => {
        set({ settings })
      },

      setError: (error: string | null) => {
        set({ error })
      },

      clearError: () => {
        set({ error: null })
      },

      setIsInitialized: (initialized: boolean) => {
        set({ isInitialized: initialized })
      },

      // 初始化
      initialize: async () => {
        const { isInitialized } = get()

        // 防止重复初始化
        if (isInitialized) return

        try {
          set({ error: null })

          const settings = await initializeWebSettings()

          set({
            settings,
            isInitialized: true,
          })

          console.log('✅ 前端设置 Store 初始化完成')
        } catch (error) {
          set({
            error: error instanceof Error ? error.message : '初始化失败',
            isInitialized: true, // 即使失败也标记为已初始化，避免重复尝试
          })
          console.error('❌ 前端设置 Store 初始化失败:', error)
        }
      },

      // 加载设置
      loadSettings: async () => {
        try {
          set({ error: null })
          const settings = await readWebSettings()

          if (settings) {
            set({ settings })
          } else {
            // 文件不存在，使用默认设置
            set({ settings: DEFAULT_WEB_SETTINGS })
          }
        } catch (error) {
          set({
            error: error instanceof Error ? error.message : '加载设置失败',
          })
          throw error
        }
      },

      // 更新背景设置（乐观更新）
      updateBackgroundSettings: async (partialBackground: Partial<WebBackgroundSettings>) => {
        const { settings } = get()
        const previousSettings = settings

        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...settings,
          ui: {
            ...settings.ui,
            background: {
              ...settings.ui.background,
              ...partialBackground,
            },
          },
        }

        set({
          settings: optimisticSettings,
          error: null,
        })

        try {
          // 2. 同步到文件
          await writeWebSettings(optimisticSettings)
          console.log('✅ 背景设置已更新:', partialBackground)
        } catch (error) {
          // 3. 失败时回滚
          set({
            settings: previousSettings,
            error: error instanceof Error ? error.message : '更新背景设置失败',
          })
          console.error('❌ 背景设置更新失败，已回滚:', error)
          throw error
        }
      },

      // 更新主题设置（乐观更新）
      updateThemeSettings: async (partialTheme: Partial<ThemeSettings>) => {
        const { settings } = get()
        const previousSettings = settings

        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...settings,
          ui: {
            ...settings.ui,
            theme: {
              ...settings.ui.theme,
              ...partialTheme,
            },
          },
        }

        set({
          settings: optimisticSettings,
          error: null,
        })

        try {
          // 2. 同步到文件
          await writeWebSettings(optimisticSettings)
          console.log('✅ 主题设置已更新:', partialTheme)
        } catch (error) {
          // 3. 失败时回滚
          set({
            settings: previousSettings,
            error: error instanceof Error ? error.message : '更新主题设置失败',
          })
          console.error('❌ 主题设置更新失败，已回滚:', error)
          throw error
        }
      },

      // 选择并设置背景图片
      selectAndSetBackgroundImage: async () => {
        try {
          const imagePath = await selectBackgroundImage()
          if (imagePath) {
            // 复制图片到资源目录
            const copiedImagePath = await copyBackgroundImageToResources(imagePath)

            // 使用复制后的路径更新设置
            await get().updateBackgroundSettings({
              type: 'image',
              imagePath: copiedImagePath,
            })

            await get().loadSettings()
          }
        } catch (error) {
          console.error('设置背景图片失败:', error)
          throw error
        }
      },

      // 移除背景图片
      removeBackgroundImage: async () => {
        try {
          await get().updateBackgroundSettings({
            type: 'none',
            imagePath: '',
          })
        } catch (error) {
          console.error('移除背景图片失败:', error)
          throw error
        }
      },

      // 重置为默认设置
      resetToDefault: async () => {
        try {
          const defaultSettings = {
            ...DEFAULT_WEB_SETTINGS,
            createdAt: get().settings.createdAt, // 保留创建时间
            updatedAt: new Date().toISOString(),
          }

          await writeWebSettings(defaultSettings)
          set({ settings: defaultSettings })
          console.log('✅ 已重置为默认设置')
        } catch (error) {
          console.error('重置设置失败:', error)
          throw error
        }
      },

      // 清理资源
      cleanup: () => {
        set({
          settings: DEFAULT_WEB_SETTINGS,
          error: null,
          isInitialized: false,
        })
        console.log('🧹 前端设置 Store 已清理')
      },
    }),
    {
      name: 'web-settings-store',
      enabled: import.meta.env.DEV,
    }
  )
)

// 页面卸载时清理资源
if (typeof window !== 'undefined') {
  window.addEventListener('beforeunload', () => {
    useWebSettingsStore.getState().cleanup()
  })
}
