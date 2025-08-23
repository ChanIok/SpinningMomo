// 配置文件路径
export const CONFIG_PATH = './web-settings.json'

// 支持的图片格式
export const SUPPORTED_IMAGE_FORMATS = [
  'jpg', 
  'jpeg', 
  'png', 
  'webp', 
  'bmp'
] as const

// 透明度范围
export const OPACITY_RANGE = {
  MIN: 0,
  MAX: 1,
  DEFAULT: 1.0,
  STEP: 0.1
} as const

// 文件对话框配置
export const IMAGE_FILE_FILTER = {
  name: '图片文件',
  extensions: SUPPORTED_IMAGE_FORMATS
} as const
