// 资源目录路径
export const RESOURCES_DIR = './resources/web/assets'

// 背景图片文件名
export const BACKGROUND_IMAGE_NAME = 'background'

// 透明度范围
export const OPACITY_RANGE = {
  MIN: 0,
  MAX: 1,
  DEFAULT: 0.8,
  STEP: 0.1,
} as const

// 模糊范围
export const BLUR_RANGE = {
  MIN: 0,
  MAX: 200,
  DEFAULT: 0,
  STEP: 1,
} as const

// === 菜单项 Registry ===

// 内置比例预设（用于快速查找，但用户可自定义任意 W:H 格式）
export const ASPECT_RATIO_PRESETS: Record<string, number> = {
  '32:9': 32 / 9,
  '21:9': 21 / 9,
  '16:9': 16 / 9,
  '3:2': 3 / 2,
  '1:1': 1,
  '3:4': 3 / 4,
  '2:3': 2 / 3,
  '9:16': 9 / 16,
}

// 内置分辨率别名
export const RESOLUTION_ALIASES: Record<string, [number, number]> = {
  Default: [0, 0],
  '480P': [720, 480],
  '720P': [1280, 720],
  '1080P': [1920, 1080],
  '2K': [2560, 1440],
  '4K': [3840, 2160],
  '5K': [5120, 2880],
  '6K': [5760, 3240],
  '8K': [7680, 4320],
  '10K': [10240, 4320],
  '12K': [11520, 6480],
  '16K': [15360, 8640],
}
