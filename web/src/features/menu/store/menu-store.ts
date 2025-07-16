import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type { MenuState, AppSettings, FeatureItem, PresetItem } from '../types'
import { DEFAULT_APP_SETTINGS } from '../types'
import { getAppSettings, updateAppSettings } from '../api/menu-api'

interface MenuStoreState extends MenuState {
  // 状态更新方法
  setAppSettings: (settings: AppSettings) => void
  setError: (error: string | null) => void
  clearError: () => void
  
  // 业务方法（使用乐观更新）
  initialize: () => Promise<void>
  updateWindowTitle: (title: string) => Promise<void>
  updateMenuItems: (items: FeatureItem[]) => Promise<void>
  updateAspectRatios: (items: PresetItem[]) => Promise<void>
  updateResolutions: (items: PresetItem[]) => Promise<void>
  loadAppSettings: () => Promise<void>
  
  // 清理方法
  cleanup: () => void
}

export const useMenuStore = create<MenuStoreState>()(
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
            isInitialized: true 
          })
          
          console.log('✅ Menu store 初始化完成')
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '初始化失败',
            isLoading: false 
          })
          console.error('❌ Menu store 初始化失败:', error)
        }
      },

      // 加载应用设置
      loadAppSettings: async () => {
        try {
          set({ isLoading: true, error: null })
          const appSettings = await getAppSettings()
          set({ 
            appSettings,
            isLoading: false 
          })
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '加载应用设置失败',
            isLoading: false 
          })
          throw error
        }
      },

      // 乐观更新：更新窗口标题
      updateWindowTitle: async (title: string) => {
        const { appSettings } = get()
        const previousSettings = appSettings
        
        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...appSettings,
          title
        }
        set({ 
          appSettings: optimisticSettings,
          error: null 
        })
        
        try {
          // 2. 同步到后端
          await updateAppSettings(optimisticSettings)
          console.log('✅ 窗口标题已更新:', title)
        } catch (error) {
          // 3. 失败时回滚到之前的状态
          set({ 
            appSettings: previousSettings,
            error: error instanceof Error ? error.message : '更新窗口标题失败'
          })
          console.error('❌ 窗口标题更新失败，已回滚:', error)
          throw error
        }
      },

      // 乐观更新：更新菜单项
      updateMenuItems: async (items: FeatureItem[]) => {
        const { appSettings } = get()
        const previousSettings = appSettings
        
        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...appSettings,
          appMenu: {
            ...appSettings.appMenu,
            menuItems: items
          }
        }
        set({ 
          appSettings: optimisticSettings,
          error: null 
        })
        
        try {
          // 2. 同步到后端
          await updateAppSettings(optimisticSettings)
          console.log('✅ 菜单项已更新:', items)
        } catch (error) {
          // 3. 失败时回滚到之前的状态
          set({ 
            appSettings: previousSettings,
            error: error instanceof Error ? error.message : '更新菜单项失败'
          })
          console.error('❌ 菜单项更新失败，已回滚:', error)
          throw error
        }
      },

      // 乐观更新：更新比例设置
      updateAspectRatios: async (items: PresetItem[]) => {
        const { appSettings } = get()
        const previousSettings = appSettings
        
        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...appSettings,
          appMenu: {
            ...appSettings.appMenu,
            aspectRatios: items
          }
        }
        set({ 
          appSettings: optimisticSettings,
          error: null 
        })
        
        try {
          // 2. 同步到后端
          await updateAppSettings(optimisticSettings)
          console.log('✅ 比例设置已更新:', items)
        } catch (error) {
          // 3. 失败时回滚到之前的状态
          set({ 
            appSettings: previousSettings,
            error: error instanceof Error ? error.message : '更新比例设置失败'
          })
          console.error('❌ 比例设置更新失败，已回滚:', error)
          throw error
        }
      },

      // 乐观更新：更新分辨率设置
      updateResolutions: async (items: PresetItem[]) => {
        const { appSettings } = get()
        const previousSettings = appSettings
        
        // 1. 立即更新本地状态（乐观更新）
        const optimisticSettings = {
          ...appSettings,
          appMenu: {
            ...appSettings.appMenu,
            resolutions: items
          }
        }
        set({ 
          appSettings: optimisticSettings,
          error: null 
        })
        
        try {
          // 2. 同步到后端
          await updateAppSettings(optimisticSettings)
          console.log('✅ 分辨率设置已更新:', items)
        } catch (error) {
          // 3. 失败时回滚到之前的状态
          set({ 
            appSettings: previousSettings,
            error: error instanceof Error ? error.message : '更新分辨率设置失败'
          })
          console.error('❌ 分辨率设置更新失败，已回滚:', error)
          throw error
        }
      },

      // 清理资源
      cleanup: () => {
        set({
          appSettings: DEFAULT_APP_SETTINGS,
          isLoading: false,
          error: null,
          isInitialized: false
        })
        console.log('🧹 Menu store 已清理')
      }
    }),
    {
      name: 'menu-store',
      // 只在开发环境启用devtools
      enabled: import.meta.env.DEV,
    }
  )
)

// 自动初始化（可选，也可以在组件中手动调用）
if (typeof window !== 'undefined') {
  // 延迟初始化，确保组件已挂载
  setTimeout(() => {
    useMenuStore.getState().initialize().catch(console.error)
  }, 100)
}

// 页面卸载时清理资源
if (typeof window !== 'undefined') {
  window.addEventListener('beforeunload', () => {
    useMenuStore.getState().cleanup()
  })
} 