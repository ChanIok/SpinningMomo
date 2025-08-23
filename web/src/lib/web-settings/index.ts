// 导出类型
export type { 
  WebSettings, 
  WebBackgroundSettings,
  WebSettingsState
} from './webSettingsTypes'

// 导出常量
export { 
  DEFAULT_WEB_SETTINGS,
  DEFAULT_BACKGROUND_SETTINGS 
} from './webSettingsTypes'

export {
  CONFIG_PATH,
  SUPPORTED_IMAGE_FORMATS,
  OPACITY_RANGE,
  IMAGE_FILE_FILTER
} from './constants'

// 导出API函数
export {
  readWebSettings,
  writeWebSettings,
  initializeWebSettings,
  selectBackgroundImage,
  checkWebSettingsExists
} from './webSettingsApi'

// 导出Store
export { useWebSettingsStore } from './webSettingsStore'
