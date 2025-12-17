
// 功能项（应用菜单中的功能）
export interface FeatureItem {
  id: string // 如: 'screenshot.capture', 'screenshot.openFolder'
  enabled: boolean // 是否显示在菜单中
  order: number // 排序序号
}

// 预设项（比例/分辨率）
export interface PresetItem {
  id: string // 如: '16:9', '1080P'
  enabled: boolean // 是否显示在菜单中
  order: number // 排序序号
}

// AppWindow主题模式
export type AppWindowThemeMode = 'dark' | 'light'

// AppWindow颜色配置
export interface AppWindowColors {
  background: string // 主背景色 (包含透明度)
  separator: string // 分隔线颜色 (包含透明度)
  text: string // 文字颜色 (包含透明度)
  indicator: string // 指示器颜色 (包含透明度)
  hover: string // 悬停背景色 (包含透明度)
  titleBar: string // 标题栏颜色 (包含透明度)
}

// 深色主题颜色配置
export const DARK_APP_WINDOW_COLORS: AppWindowColors = {
  background: '#282C34A6',
  separator: '#333842A6',
  text: '#DEE8FCFF',
  indicator: '#61AFEFFF',
  hover: '#383D4AA6',
  titleBar: '#282C34A6',
}

// 浅色主题颜色配置
export const LIGHT_APP_WINDOW_COLORS: AppWindowColors = {
  background: '#F9FAFBCC',
  separator: '#E4E5E7CC',
  text: '#1F2937FF',
  indicator: '#FFAF50FF',
  hover: '#F2F2F2CC',
  titleBar: '#F9FAFBCC',
}

// AppWindow布局配置
export interface AppWindowLayout {
  baseItemHeight: number
  baseTitleHeight: number
  baseSeparatorHeight: number
  baseFontSize: number
  baseTextPadding: number
  baseIndicatorWidth: number
  baseRatioIndicatorWidth: number
  baseRatioColumnWidth: number
  baseResolutionColumnWidth: number
  baseSettingsColumnWidth: number
}

// 完整的应用设置类型
export interface AppSettings {
  version: string

  // app 分组 - 应用核心设置
  app: {
    // 快捷键设置
    hotkey: {
      toggleVisibility: {
        modifiers: number // Ctrl + Alt = 3
        key: number // R键 = 82
      }
      screenshot: {
        modifiers: number // 无修饰键 = 0
        key: number // 印屏键 = 44
      }
    }

    // 语言设置
    language: {
      current: string // zh-CN, en-US
    }

    // 日志设置
    logger: {
      level: string // DEBUG, INFO, ERROR
    }
  }

  // window 分组 - 窗口相关设置
  window: {
    targetTitle: string // 目标窗口标题

    // 任务栏设置
    taskbar: {
      autoHide: boolean // 隐藏任务栏
      lowerOnResize: boolean // 调整时置底任务栏
    }
  }

  // features 分组 - 功能特性设置
  features: {
    // 截图功能设置
    screenshot: {
      screenshotDirPath: string // 截图目录路径
    }

    // 黑边模式设置
    letterbox: {
      enabled: boolean // 是否启用黑边模式
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

    // AppWindow布局配置
    appWindowLayout: AppWindowLayout

    // AppWindow颜色配置
    appWindowColors: AppWindowColors

    // AppWindow主题模式
    appWindowThemeMode: AppWindowThemeMode
  }
}

// 默认设置值
export const DEFAULT_APP_SETTINGS: AppSettings = {
  version: '1.0',

  // app 设置
  app: {
    hotkey: {
      toggleVisibility: {
        modifiers: 3, // Ctrl + Alt
        key: 82, // R键
      },
      screenshot: {
        modifiers: 0, // 无修饰键
        key: 44, // 印屏键
      },
    },
    language: {
      current: 'zh-CN',
    },
    logger: {
      level: 'INFO',
    },
  },

  // window 设置
  window: {
    targetTitle: '无限暖暖  ',
    taskbar: {
      autoHide: false,
      lowerOnResize: true,
    },
  },

  // features 设置
  features: {
    screenshot: {
      screenshotDirPath: '',
    },
    letterbox: {
      enabled: false,
    },
  },

  // ui 设置
  ui: {
    appMenu: {
      featureItems: [
        { id: 'screenshot.capture', enabled: true, order: 1 },
        { id: 'screenshot.open_folder', enabled: true, order: 2 },
        { id: 'feature.toggle_preview', enabled: true, order: 3 },
        { id: 'feature.toggle_overlay', enabled: true, order: 4 },
        { id: 'feature.toggle_letterbox', enabled: true, order: 5 },
        { id: 'window.reset_transform', enabled: true, order: 6 },
        { id: 'panel.hide', enabled: false, order: 7 },
        { id: 'app.exit', enabled: true, order: 8 },
      ],
      aspectRatios: [
        { id: '32:9', enabled: false, order: 1 },
        { id: '21:9', enabled: true, order: 2 },
        { id: '16:9', enabled: true, order: 3 },
        { id: '3:2', enabled: true, order: 4 },
        { id: '1:1', enabled: true, order: 5 },
        { id: '3:4', enabled: true, order: 6 },
        { id: '2:3', enabled: true, order: 7 },
        { id: '9:16', enabled: true, order: 8 },
      ],
      resolutions: [
        { id: 'Default', enabled: true, order: 1 },
        { id: '1080P', enabled: true, order: 2 },
        { id: '2K', enabled: true, order: 3 },
        { id: '4K', enabled: true, order: 4 },
        { id: '6K', enabled: true, order: 5 },
        { id: '8K', enabled: true, order: 6 },
        { id: '12K', enabled: true, order: 7 },
      ],
    },
    appWindowLayout: {
      baseItemHeight: 24,
      baseTitleHeight: 26,
      baseSeparatorHeight: 1,
      baseFontSize: 12,
      baseTextPadding: 12,
      baseIndicatorWidth: 3,
      baseRatioIndicatorWidth: 4,
      baseRatioColumnWidth: 60,
      baseResolutionColumnWidth: 70,
      baseSettingsColumnWidth: 80,
    },
    appWindowColors: DARK_APP_WINDOW_COLORS,
    appWindowThemeMode: 'dark',
  },
} as const
