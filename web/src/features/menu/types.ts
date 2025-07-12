// 菜单页面类型
export type MenuPage = 'main'

// 窗口设置类型
export interface WindowSettings {
  title: string
}

// RPC方法类型定义
export interface MenuRPCGetSettings {
  method: 'settings.get'
  params: void
  result: {
    window: WindowSettings
    version: string
  }
}

export interface MenuRPCUpdateSettings {
  method: 'settings.update'
  params: {
    window: WindowSettings
  }
  result: {
    success: boolean
    message: string
  }
}

export type MenuRPCMethods = MenuRPCGetSettings | MenuRPCUpdateSettings

// 菜单状态类型
export interface MenuState {
  windowTitle: string
  isLoading: boolean
  error: string | null
  isInitialized: boolean
}

// 默认设置值
export const DEFAULT_WINDOW_SETTINGS: WindowSettings = {
  title: ''
} as const 