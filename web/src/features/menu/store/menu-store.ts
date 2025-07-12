import { create } from 'zustand'
import { devtools } from 'zustand/middleware'
import type { MenuState } from '../types'
import { getWindowSettings, updateWindowTitle as apiUpdateWindowTitle } from '../api/menu-api'

interface MenuStoreState extends MenuState {
  // 状态更新方法
  setWindowTitle: (title: string) => void
  setLoading: (loading: boolean) => void
  setError: (error: string | null) => void
  clearError: () => void
  
  // 业务方法
  initialize: () => Promise<void>
  updateWindowTitle: (title: string) => Promise<void>
  loadWindowSettings: () => Promise<void>
  
  // 清理方法
  cleanup: () => void
}

export const useMenuStore = create<MenuStoreState>()(
  devtools(
    (set, get) => ({
      // 初始状态
      windowTitle: '',
      isLoading: false,
      error: null,
      isInitialized: false,

      // 状态更新方法
      setWindowTitle: (title: string) => {
        set({ windowTitle: title })
      },

      setLoading: (loading: boolean) => {
        set({ isLoading: loading })
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

          // 加载窗口设置
          const windowSettings = await getWindowSettings()
          
          set({ 
            windowTitle: windowSettings.title,
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

      // 加载窗口设置
      loadWindowSettings: async () => {
        try {
          set({ isLoading: true, error: null })
          const windowSettings = await getWindowSettings()
          set({ 
            windowTitle: windowSettings.title,
            isLoading: false 
          })
        } catch (error) {
          set({ 
            error: error instanceof Error ? error.message : '加载窗口设置失败',
            isLoading: false 
          })
          throw error
        }
      },

      // 更新窗口标题
      updateWindowTitle: async (title: string) => {
        try {
          set({ isLoading: true, error: null })
          
          // 更新到后端
          await apiUpdateWindowTitle(title)
          
          // 更新本地状态
          set({
            windowTitle: title,
            isLoading: false
          })
          
          console.log('✅ 窗口标题已更新:', title)
        } catch (error) {
          set({
            error: error instanceof Error ? error.message : '更新窗口标题失败',
            isLoading: false
          })
          console.error('❌ 窗口标题更新失败:', error)
          throw error
        }
      },

      // 清理资源
      cleanup: () => {
        set({
          windowTitle: '',
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