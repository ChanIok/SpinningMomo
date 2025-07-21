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

// 应用设置类型
export interface AppSettings {
  version: string
  
  // app 分组 - 应用核心设置
  app: {
    // 快捷键设置
    hotkey: {
      modifiers: number  // Ctrl + Alt = 3
      key: number        // R键 = 82
    }
    
    // 语言设置
    language: {
      current: string    // zh-CN, en-US
    }
    
    // 日志设置
    logger: {
      level: string      // DEBUG, INFO, ERROR
    }
  }
  
  // window 分组 - 窗口相关设置
  window: {
    targetTitle: string  // 目标窗口标题
    
    // 任务栏设置
    taskbar: {
      autoHide: boolean       // 隐藏任务栏
      lowerOnResize: boolean  // 调整时置底任务栏
    }
  }
  
  // features 分组 - 功能特性设置
  features: {
    // 截图功能设置
    screenshot: {
      gameAlbumPath: string  // 游戏相册目录路径
    }
    
    // 黑边模式设置
    letterbox: {
      enabled: boolean  // 是否启用黑边模式
    }
  }
  
  // ui 分组 - UI界面设置
  ui: {
    // 应用菜单配置
    appMenu: {
      featureItems: FeatureItem[]
      aspectRatios: PresetItem[]
      resolutions: PresetItem[]
    }
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

// 默认设置值（匹配后端的create_default_app_settings）
export const DEFAULT_APP_SETTINGS: AppSettings = {
  version: '1.0',
  
  // app 设置
  app: {
    hotkey: {
      modifiers: 3,  // Ctrl + Alt
      key: 82        // R键
    },
    language: {
      current: 'zh-CN'
    },
    logger: {
      level: 'INFO'
    }
  },
  
  // window 设置
  window: {
    targetTitle: '',
    taskbar: {
      autoHide: false,
      lowerOnResize: true
    }
  },
  
  // features 设置
  features: {
    screenshot: {
      gameAlbumPath: ''
    },
    letterbox: {
      enabled: false
    }
  },
  
  // ui 设置
  ui: {
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
  }
} as const 