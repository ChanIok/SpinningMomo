// 重新导出共享的类型
export type { AppWindowLayout, AppSettings } from '@/lib/settings'

// 布局页面类型
export type LayoutPage = 'main'

// 布局状态类型（现在使用共享的SettingsState）
export type LayoutState = Record<string, never>