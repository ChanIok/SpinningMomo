// 菜单页面类型
export type MenuPage = 'main'

// 功能项（应用菜单中的功能）
export interface FeatureItem {
  id: string; // 如: 'screenshot.capture', 'screenshot.openFolder'
  label: string; // 显示名称
  enabled: boolean; // 是否显示在菜单中
  order: number     // 排序序号
}

// 预设项（比例/分辨率）
export interface PresetItem {
  id: string        // 如: '16:9', '1080P'
  label: string     // 显示名称
  enabled: boolean  // 是否显示在菜单中
  order: number     // 排序序号
  isCustom: boolean // 是否为自定义项
}

// 应用设置类型（与后端AppSettings匹配）
export interface AppSettings {
  version: string
  title: string
  appMenu: {
    featureItems: FeatureItem[]
    aspectRatios: PresetItem[]
    resolutions: PresetItem[]
  }
}

// RPC方法类型定义（适配新的统一接口）
export interface MenuRPCGetSettings {
  method: 'settings.get'
  params: void
  result: AppSettings
}

export interface MenuRPCUpdateSettings {
  method: 'settings.update'
  params: AppSettings
  result: {
    success: boolean
    message: string
  }
}

export type MenuRPCMethods = MenuRPCGetSettings | MenuRPCUpdateSettings

// 菜单状态类型
export interface MenuState {
  appSettings: AppSettings
  isLoading: boolean
  error: string | null
  isInitialized: boolean
}

// 默认设置值
export const DEFAULT_APP_SETTINGS: AppSettings = {
  version: '1.0',
  title: '',
  appMenu: {
    featureItems: [
      { id: 'screenshot.capture', label: '截图', enabled: true, order: 1 },
      { id: 'screenshot.openFolder', label: '打开相册', enabled: true, order: 2 },
      { id: 'feature.togglePreview', label: '预览窗口', enabled: true, order: 3 },
      { id: 'feature.toggleOverlay', label: '叠加层', enabled: true, order: 4 },
      { id: 'feature.toggleLetterbox', label: '黑边模式', enabled: true, order: 5 },
      { id: 'window.resetTransform', label: '重置窗口', enabled: true, order: 6 },
      { id: 'panel.hide', label: '关闭浮窗', enabled: true, order: 7 },
      { id: 'app.exit', label: '退出', enabled: false, order: 8 },
    ],
    aspectRatios: [
      { id: '32:9', label: '32:9', enabled: true, order: 1, isCustom: false },
      { id: '21:9', label: '21:9', enabled: true, order: 2, isCustom: false },
      { id: '16:9', label: '16:9', enabled: true, order: 3, isCustom: false },
      { id: '3:2', label: '3:2', enabled: true, order: 4, isCustom: false },
      { id: '1:1', label: '1:1', enabled: true, order: 5, isCustom: false },
      { id: '3:4', label: '3:4', enabled: true, order: 6, isCustom: false },
      { id: '2:3', label: '2:3', enabled: true, order: 7, isCustom: false },
      { id: '9:16', label: '9:16', enabled: true, order: 8, isCustom: false }
    ],
    resolutions: [
      { id: 'Default', label: 'Default', enabled: true, order: 1, isCustom: false },
      { id: '1080P', label: '1080P', enabled: true, order: 2, isCustom: false },
      { id: '2K', label: '2K', enabled: true, order: 3, isCustom: false },
      { id: '4K', label: '4K', enabled: true, order: 4, isCustom: false },
      { id: '6K', label: '6K', enabled: true, order: 5, isCustom: false },
      { id: '8K', label: '8K', enabled: true, order: 6, isCustom: false },
      { id: '12K', label: '12K', enabled: true, order: 7, isCustom: false }
    ]
  }
} as const 