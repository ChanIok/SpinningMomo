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

// 浮窗主题模式
export type FloatingWindowThemeMode = 'dark' | 'light'

// 浮窗颜色配置
export interface FloatingWindowColors {
  background: string // 主背景色 (包含透明度)
  separator: string // 分隔线颜色 (包含透明度)
  text: string // 文字颜色 (包含透明度)
  indicator: string // 指示器颜色 (包含透明度)
  hover: string // 悬停背景色 (包含透明度)
  titleBar: string // 标题栏颜色 (包含透明度)
  scrollIndicator: string // 滚动条颜色 (包含透明度)
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
export const DARK_FLOATING_WINDOW_COLORS: FloatingWindowColors = {
  background: '#1f1f1fB3',
  separator: '#333333B3',
  text: '#D8D8D8FF',
  indicator: '#FBBF24FF',
  hover: '#505050CC',
  titleBar: '#1f1f1fB3',
  scrollIndicator: '#808080CC',
}

// 浅色主题颜色配置
export const LIGHT_FLOATING_WINDOW_COLORS: FloatingWindowColors = {
  background: '#F5F5F5CC',
  separator: '#E5E5E5CC',
  text: '#2E2E2EFF',
  indicator: '#F59E0BFF',
  hover: '#E5E5E5CC',
  titleBar: '#F5F5F5CC',
  scrollIndicator: '#BDBDBDCC',
}

// 浮窗布局配置
export interface FloatingWindowLayout {
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
  baseScrollIndicatorWidth: number
}

// 完整的应用设置类型
export interface AppSettings {
  version: number

  // app 分组 - 应用核心设置
  app: {
    // 始终以管理员权限运行
    alwaysRunAsAdmin: boolean

    // 快捷键设置
    hotkey: {
      floatingWindow: {
        modifiers: number // MOD_CONTROL = 2
        key: number // VK_OEM_3 (`) = 192
      }
      screenshot: {
        modifiers: number // 无修饰键 = 0
        key: number // VK_F11 = 122
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
    outputDirPath: string // 统一输出目录（截图+录制），空=默认 Videos/SpinningMomo

    // 截图功能设置
    screenshot: {
      gameAlbumPath: string // 游戏相册目录路径
    }

    // 黑边模式设置
    letterbox: {
      enabled: boolean // 是否启用黑边模式
    }

    // 动态照片设置
    motionPhoto: {
      duration: number // 视频时长（秒）
      resolution: number // 短边分辨率: 720/1080/1440/2160
      fps: number // 帧率
      bitrate: number // 比特率
      codec: 'h264' | 'h265' // 编码格式
      audioSource: 'none' | 'system' | 'game_only' // 音频源
      audioBitrate: number // 音频码率
    }

    // 即时回放设置（录制参数继承自 recording）
    replayBuffer: {
      duration: number // 回放时长（秒）
    }

    // 录制功能设置
    recording: {
      fps: number // 帧率: 30, 60, 120
      bitrate: number // 比特率 (bps)，CBR 模式使用
      quality: number // 质量值 (0-100)，VBR 模式使用
      qp: number // 量化参数 (0-51)，ManualQP 模式使用
      rateControl: 'cbr' | 'vbr' | 'manual_qp' // 码率控制模式（默认 VBR）
      encoderMode: 'auto' | 'gpu' | 'cpu' // 编码器模式
      codec: 'h264' | 'h265' // 视频编码格式
      audioSource: 'none' | 'system' | 'game_only' // 音频源
      audioBitrate: number // 音频码率 (bps)
    }
  }

  // update 分组 - 更新设置
  update: {
    autoCheck: boolean // 是否自动检查更新
    checkIntervalHours: number // 检查间隔（小时）
    versionUrl: string // 版本检查URL（Cloudflare Pages）
    downloadSources: Array<{
      name: string // 源名称
      urlTemplate: string // URL模板，支持 {version} 和 {filename} 占位符
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

    // 浮窗布局配置
    floatingWindowLayout: FloatingWindowLayout

    // 浮窗颜色配置
    floatingWindowColors: FloatingWindowColors

    // 浮窗主题模式
    floatingWindowThemeMode: FloatingWindowThemeMode

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
    alwaysRunAsAdmin: true,
    hotkey: {
      floatingWindow: {
        modifiers: 2, // MOD_CONTROL
        key: 192, // VK_OEM_3 (`)
      },
      screenshot: {
        modifiers: 0, // 无修饰键
        key: 122, // VK_F11
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
    outputDirPath: '',
    screenshot: {
      gameAlbumPath: '',
    },
    letterbox: {
      enabled: false,
    },
    motionPhoto: {
      duration: 3,
      resolution: 1080,
      fps: 30,
      bitrate: 10000000,
      codec: 'h264',
      audioSource: 'system',
      audioBitrate: 128000,
    },
    replayBuffer: {
      duration: 30,
    },
    recording: {
      fps: 60,
      bitrate: 80000000,
      quality: 100,
      qp: 23,
      rateControl: 'vbr',
      encoderMode: 'auto',
      codec: 'h264',
      audioSource: 'system',
      audioBitrate: 320000,
    },
  },

  // update 设置
  update: {
    autoCheck: true,
    checkIntervalHours: 24,
    versionUrl: 'https://spinning-momo.pages.dev/version.txt',
    downloadSources: [
      {
        name: 'GitHub',
        urlTemplate: 'https://github.com/ChanIok/SpinningMomo/releases/download/v{0}/{1}',
      },
      {
        name: 'Mirror',
        urlTemplate: 'https://r2.spinning-momo.pages.dev/releases/v{0}/{1}',
      },
    ],
  },

  // ui 设置
  ui: {
    appMenu: {
      features: [
        'screenshot.capture',
        'motionPhoto.toggle',
        'replay_buffer.toggle',
        'replay_buffer.save',
        'recording.toggle',
        'preview.toggle',
        'overlay.toggle',
        'window.reset',
        'app.main',
        'app.exit',
        'screenshot.open_folder',
        'letterbox.toggle',
      ],
      aspectRatios: ['21:9', '16:9', '3:2', '1:1', '3:4', '2:3', '9:16'],
      resolutions: ['Default', '1080P', '2K', '4K', '6K', '8K', '12K'],
    },
    floatingWindowLayout: {
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
      baseScrollIndicatorWidth: 3,
    },
    floatingWindowColors: DARK_FLOATING_WINDOW_COLORS,
    floatingWindowThemeMode: 'dark',
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
