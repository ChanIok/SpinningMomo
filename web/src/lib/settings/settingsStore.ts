import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type { SettingsState, AppSettings } from './settingsTypes'
import { DEFAULT_APP_SETTINGS } from './settingsTypes'
import { getAppSettings, updateAppSettings } from './settingsApi'

interface SettingsStoreState extends SettingsState {
  // 状态更新方法
  setAppSettings: (settings: AppSettings) => void
  setError: (error: string | null) => void
  clearError: () => void

  // 业务方法（使用乐观更新）
  initialize: () => Promise<void>
  updateSettings: (settings: Partial<AppSettings>) => Promise<void>
  loadAppSettings: () => Promise<void>
}

export const useSettingsStore = create<SettingsStoreState>()(
  devtools(
    (set, get) => ({
      // 初始状态
      appSettings: DEFAULT_APP_SETTINGS,
      isLoading: false,
      error: null,
      isInitialized: false,

      // 状态更新方法
      setAppSettings: (settings: AppSettings) => {
        set({ appSettings: settings })
      },

      setError: (error: string | null) => {
        set({ error })
      },

      clearError: () => {
        set({ error: null })
      },

      // 初始化store
      initialize: async () => {
        const { isInitialized } = get()
        if (isInitialized) return

        try {
          set({ isLoading: true, error: null })

          // 加载应用设置
          const appSettings = await getAppSettings()

          set({
            appSettings,
            isLoading: false,
            isInitialized: true,
          })

          console.log('✅ Settings store 初始化完成')
        } catch (error) {
          set({
            error: error instanceof Error ? error.message : '初始化失败',
            isLoading: false,
          })
          console.error('❌ Settings store 初始化失败:', error)
        }
      },

      // 加载应用设置
      loadAppSettings: async () => {
        try {
          set({ isLoading: true, error: null })
          const appSettings = await getAppSettings()
          set({
            appSettings,
            isLoading: false,
          })
        } catch (error) {
          set({
            error: error instanceof Error ? error.message : '加载应用设置失败',
            isLoading: false,
          })
          throw error
        }
      },

      // 乐观更新：更新应用设置（支持部分更新）
      updateSettings: async (partialSettings: Partial<AppSettings>) => {
        const { appSettings } = get()
        const previousSettings = appSettings

        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...appSettings,
          ...partialSettings,
          // 特殊处理嵌套对象
          app: {
            ...appSettings.app,
            ...(partialSettings.app || {}),
          },
          window: {
            ...appSettings.window,
            ...(partialSettings.window || {}),
          },
          features: {
            ...appSettings.features,
            ...(partialSettings.features || {}),
          },
          ui: {
            ...appSettings.ui,
            ...(partialSettings.ui || {}),
            appMenu: {
              ...appSettings.ui.appMenu,
              ...(partialSettings.ui?.appMenu || {}),
            },
            appWindowLayout: {
              ...appSettings.ui.appWindowLayout,
              ...(partialSettings.ui?.appWindowLayout || {}),
            },
          },
        }

        set({
          appSettings: optimisticSettings,
          error: null,
        })

        try {
          // 2. 同步到后端
          await updateAppSettings(optimisticSettings)
          console.log('✅ 应用设置已更新:', partialSettings)
        } catch (error) {
          // 3. 失败时回滚到之前的状态
          set({
            appSettings: previousSettings,
            error: error instanceof Error ? error.message : '更新应用设置失败',
          })
          console.error('❌ 应用设置更新失败，已回滚:', error)
          throw error
        }
      },
    }),
    {
      name: 'settings-store',
      // 只在开发环境启用devtools
      enabled: import.meta.env.DEV,
    }
  )
)

// 自动初始化（可选，也可以在组件中手动调用）
if (typeof window !== 'undefined') {
  // 延迟初始化，确保组件已挂载
  setTimeout(() => {
    useSettingsStore.getState().initialize().catch(console.error)
  }, 100)
}
