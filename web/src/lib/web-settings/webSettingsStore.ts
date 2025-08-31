import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type { WebSettings, WebSettingsState } from './webSettingsTypes'
import { DEFAULT_WEB_SETTINGS } from './webSettingsTypes'
import { readWebSettings, writeWebSettings, initializeWebSettings } from './webSettingsApi'

interface WebSettingsActions {
  // 基础操作
  setSettings: (settings: WebSettings) => void
  setError: (error: string | null) => void
  clearError: () => void
  setIsInitialized: (initialized: boolean) => void

  // 业务操作
  initialize: () => Promise<void>
  updateSettings: (settings: Partial<WebSettings>) => Promise<void>
  loadSettings: () => Promise<void>
  resetToDefault: () => Promise<void>
}

type WebSettingsStoreType = WebSettingsState & WebSettingsActions

export const useWebSettingsStore = create<WebSettingsStoreType>()(
  devtools(
    (set, get) => ({
      // 初始状态
      webSettings: DEFAULT_WEB_SETTINGS,
      error: null,
      isInitialized: false,

      // 基础操作
      setSettings: (settings: WebSettings) => {
        set({ webSettings: settings })
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

          const webSettings = await initializeWebSettings()

          set({
            webSettings,
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
          const webSettings = await readWebSettings()

          if (webSettings) {
            set({ webSettings })
          } else {
            // 文件不存在，使用默认设置
            set({ webSettings: DEFAULT_WEB_SETTINGS })
          }
        } catch (error) {
          set({
            error: error instanceof Error ? error.message : '加载设置失败',
          })
          throw error
        }
      },

      // 乐观更新：更新设置（支持部分更新）
      updateSettings: async (partialSettings: Partial<WebSettings>) => {
        const { webSettings } = get()
        const previousSettings = webSettings

        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...webSettings,
          ...partialSettings,
          // 特殊处理嵌套对象
          ui: {
            ...webSettings.ui,
            ...(partialSettings.ui || {}),
            background: {
              ...webSettings.ui.background,
              ...(partialSettings.ui?.background || {}),
            },
            theme: {
              ...webSettings.ui.theme,
              ...(partialSettings.ui?.theme || {}),
            },
          },
        }

        set({
          webSettings: optimisticSettings,
          error: null,
        })

        try {
          // 2. 同步到文件
          await writeWebSettings(optimisticSettings)
          console.log('✅ Web设置已更新:', partialSettings)
        } catch (error) {
          // 3. 失败时回滚
          set({
            webSettings: previousSettings,
            error: error instanceof Error ? error.message : '更新Web设置失败',
          })
          console.error('❌ Web设置更新失败，已回滚:', error)
          throw error
        }
      },

      // 重置为默认设置
      resetToDefault: async () => {
        try {
          const defaultSettings = {
            ...DEFAULT_WEB_SETTINGS,
            createdAt: get().webSettings.createdAt, // 保留创建时间
            updatedAt: new Date().toISOString(),
          }

          await writeWebSettings(defaultSettings)
          set({ webSettings: defaultSettings })
          console.log('✅ 已重置为默认设置')
        } catch (error) {
          console.error('重置设置失败:', error)
          throw error
        }
      },
    }),
    {
      name: 'web-settings-store',
      enabled: import.meta.env.DEV,
    }
  )
)
