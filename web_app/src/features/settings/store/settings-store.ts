import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type { AppSettings, AppInfo, SettingsPage, PartialAppSettings } from '../types'
import { settingsAPI } from '../api/settings-api'
import type { RpcEventHandler } from '@/lib/webview-rpc'

interface SettingsState {

  isLoading: boolean
  error: string | null
  currentPage: SettingsPage
  settings: AppSettings | null
  appInfo: AppInfo | null
  isInitialized: boolean

  setCurrentPage: (page: SettingsPage) => void
  initialize: () => Promise<void>
  loadSettings: () => Promise<void>
  loadAppInfo: () => Promise<void>
  updateSettings: (updates: PartialAppSettings) => Promise<void>
  resetSettings: () => Promise<void>
  clearError: () => void
  cleanup: () => void
}

// 设置变化事件处理器
let settingsChangeHandler: RpcEventHandler<AppSettings> | null = null

export const useSettingsStore = create<SettingsState>()(
  devtools(
    (set, get) => ({
      // 初始状态
      isLoading: false,
      error: null,
      currentPage: 'general',
      settings: null,
      appInfo: null,
      isInitialized: false,

      // 初始化store
      initialize: async () => {
        const { isInitialized } = get()
        if (isInitialized) return

        try {
          set({ isLoading: true, error: null })

          // 并行加载设置和应用信息
          const [settings, appInfo] = await Promise.all([
            settingsAPI.getSettings(),
            settingsAPI.getAppInfo().catch(error => {
              console.warn('Failed to load app info:', error)
              return null // 应用信息加载失败不影响主要功能
            })
          ])

          // 设置监听器
          if (!settingsChangeHandler) {
            settingsChangeHandler = (newSettings: AppSettings) => {
              const currentState = get()
              if (currentState.isInitialized) {
                set({ settings: newSettings })
                console.log('🔄 设置已从C++端更新:', newSettings)
              }
            }
            settingsAPI.onSettingsChanged(settingsChangeHandler)
          }

          set({ 
            settings, 
            appInfo, 
            isLoading: false, 
            isInitialized: true 
          })
          
          console.log('✅ Settings store 初始化完成')
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '初始化失败',
            isLoading: false 
          })
          console.error('❌ Settings store 初始化失败:', error)
        }
      },

      // 设置当前页面
      setCurrentPage: (page) => {
        set({ currentPage: page })
      },

      // 重新加载设置
      loadSettings: async () => {
        try {
          set({ isLoading: true, error: null })
          const settings = await settingsAPI.getSettings()
          set({ settings, isLoading: false })
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '加载设置失败',
            isLoading: false 
          })
          throw error
        }
      },

      // 重新加载应用信息
      loadAppInfo: async () => {
        try {
          const appInfo = await settingsAPI.getAppInfo()
          set({ appInfo })
        } catch (error) {
          console.error('Failed to load app info:', error)
          // 应用信息加载失败不抛出错误，不影响主要功能
        }
      },

      // 更新设置（优化合并逻辑）
      updateSettings: async (updates) => {
        const { settings } = get()
        if (!settings) {
          throw new Error('设置尚未初始化')
        }

        // 深度合并设置
        const updatedSettings: AppSettings = {
          general: {
            ...settings.general,
            ...updates.general,
          },
          advanced: {
            ...settings.advanced,
            ...updates.advanced,
          },
        }

        try {
          set({ isLoading: true, error: null })
          
          // 保存到后端
          await settingsAPI.saveSettings(updates)
          
          // 立即更新本地状态（如果没有监听器的话）
          if (!settingsChangeHandler) {
            set({ settings: updatedSettings })
          }
          
          set({ isLoading: false })
          console.log('✅ 设置已保存:', updates)
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '保存设置失败',
            isLoading: false 
          })
          console.error('❌ 设置保存失败:', error)
          throw error
        }
      },

      // 重置设置
      resetSettings: async () => {
        try {
          set({ isLoading: true, error: null })
          const defaultSettings = await settingsAPI.resetSettings()
          
          // 立即更新本地状态（如果没有监听器的话）
          if (!settingsChangeHandler) {
            set({ settings: defaultSettings })
          }
          
          set({ isLoading: false })
          console.log('✅ 设置已重置为默认值')
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '重置设置失败',
            isLoading: false 
          })
          console.error('❌ 设置重置失败:', error)
          throw error
        }
      },

      // 清除错误
      clearError: () => {
        set({ error: null })
      },

      // 清理资源
      cleanup: () => {
        if (settingsChangeHandler) {
          settingsAPI.offSettingsChanged(settingsChangeHandler)
          settingsChangeHandler = null
          console.log('🧹 Settings store 已清理')
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

// 页面卸载时清理资源
if (typeof window !== 'undefined') {
  window.addEventListener('beforeunload', () => {
    useSettingsStore.getState().cleanup()
  })
}

 