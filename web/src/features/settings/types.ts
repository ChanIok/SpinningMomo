// 功能描述符（从后端获取）
export interface FeatureDescriptor {
  id: string // 唯一标识
  i18nKey: string // i18n 键
  isToggle: boolean // 是否为切换类型
}

// 菜单项（用于显示和编辑）
export interface MenuItem {
  id: string // 项目 ID
  enabled: boolean // 是否启用
  order?: number // 显示顺序（-1 表示未启用/无顺序）
}

// Web 主题模式（页面主题）
export type WebThemeMode = 'light' | 'dark' | 'system'

// AppWindow主题模式（原生窗口主题）
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

// Web 背景设置
export interface WebBackgroundSettings {
  type: 'none' | 'image'
  imagePath: string
  opacity: number
  blurAmount: number
}

// Web 主题设置
export interface WebThemeSettings {
  mode: WebThemeMode
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
  version: number

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

    // 录制功能设置
    recording: {
      outputDirPath: string // 输出目录（空=默认使用 exe/recordings/）
      fps: number // 帧率: 30, 60, 120
      bitrate: number // 比特率 (bps)
      encoderMode: 'auto' | 'gpu' | 'cpu' // 编码器模式
    }
  }

  // updater 分组 - 更新设置
  updater: {
    autoCheck: boolean // 是否自动检查更新
    checkIntervalHours: number // 检查间隔（小时）
    servers: Array<{
      name: string // 服务器名称
      url: string // 服务器API地址
    }>
  }

  // ui 分组 - UI界面设置
  ui: {
    // 应用菜单配置
    appMenu: {
      features: string[] // 启用的功能项（有则启用，顺序即菜单显示顺序）
      aspectRatios: string[] // 启用的比例列表
      resolutions: string[] // 启用的分辨率列表
    }

    // AppWindow布局配置
    appWindowLayout: AppWindowLayout

    // AppWindow颜色配置
    appWindowColors: AppWindowColors

    // AppWindow主题模式
    appWindowThemeMode: AppWindowThemeMode

    // Web UI 设置
    webTheme: WebThemeSettings
    background: WebBackgroundSettings
  }
}

// 默认设置值
export const DEFAULT_APP_SETTINGS: AppSettings = {
  version: 1,

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
    recording: {
      outputDirPath: '',
      fps: 60,
      bitrate: 80000000,
      encoderMode: 'auto',
    },
  },

  // updater 设置
  updater: {
    autoCheck: true,
    checkIntervalHours: 24,
    servers: [
      {
        name: 'GitHub官方',
        url: 'https://api.github.com/repos/ChanIok/SpinningMomo/releases/latest',
      },
    ],
  },

  // ui 设置
  ui: {
    appMenu: {
      features: [
        'screenshot.capture',
        'screenshot.open_folder',
        'feature.toggle_preview',
        'feature.toggle_overlay',
        'feature.toggle_letterbox',
        'feature.toggle_recording',
        'window.reset_transform',
        'app.exit',
      ],
      aspectRatios: ['21:9', '16:9', '3:2', '1:1', '3:4', '2:3', '9:16'],
      resolutions: ['Default', '1080P', '2K', '4K', '6K', '8K', '12K'],
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
    webTheme: {
      mode: 'system',
    },
    background: {
      type: 'none',
      imagePath: '',
      opacity: 0.8,
      blurAmount: 0,
    },
  },
} as const
