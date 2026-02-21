import { createApp } from 'vue'
import { watch } from 'vue'
import { createPinia } from 'pinia'
import router from './router'
import { setupRouterGuards } from './router/guards'
import { initializeRPC } from '@/core/rpc'
import { initI18n } from '@/core/i18n'
import { useSettingsStore } from '@/features/settings/store'
import { applyAppearanceToDocument, preloadBackgroundImage } from '@/features/settings/appearance'
import './index.css'
import App from './App.vue'

// 创建 Pinia 实例
const pinia = createPinia()

const app = createApp(App)

// 注册插件
app.use(pinia)
app.use(router)

// 设置路由守卫
setupRouterGuards(router)

// 初始化 RPC 通信
initializeRPC()

// 初始化应用
;(async () => {
  // 首先初始化 i18n（使用默认语言）
  await initI18n('zh-CN')

  // 然后初始化 settings store，它会自动同步后端的语言设置
  const settingsStore = useSettingsStore()
  await settingsStore.init()

  // 在挂载前应用主题和背景，避免首屏闪烁
  applyAppearanceToDocument(settingsStore.appSettings)
  preloadBackgroundImage(settingsStore.appSettings)

  // 监听设置变化，实时同步外观
  watch(
    () => [
      settingsStore.appSettings.ui.webTheme.mode,
      settingsStore.appSettings.ui.background.type,
      settingsStore.appSettings.ui.background.imagePath,
      settingsStore.appSettings.ui.background.backgroundBlurAmount,
      settingsStore.appSettings.ui.background.backgroundOpacity,
      settingsStore.appSettings.ui.background.overlayStartColor,
      settingsStore.appSettings.ui.background.overlayEndColor,
      settingsStore.appSettings.ui.background.overlayOpacity,
      settingsStore.appSettings.ui.background.surfaceBlurAmount,
      settingsStore.appSettings.ui.background.surfaceOpacity,
    ],
    () => {
      applyAppearanceToDocument(settingsStore.appSettings)
    }
  )

  // 仅在背景图源变更时预加载
  watch(
    () =>
      `${settingsStore.appSettings.ui.background.type}|${settingsStore.appSettings.ui.background.imagePath}`,
    () => {
      preloadBackgroundImage(settingsStore.appSettings)
    }
  )

  // 监听系统主题变化（仅在跟随系统模式下生效）
  if (typeof window !== 'undefined') {
    const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)')
    const handleSchemeChange = () => {
      if (settingsStore.appSettings.ui.webTheme.mode === 'system') {
        applyAppearanceToDocument(settingsStore.appSettings)
      }
    }

    if (mediaQuery.addEventListener) {
      mediaQuery.addEventListener('change', handleSchemeChange)
    } else {
      mediaQuery.addListener(handleSchemeChange)
    }
  }

  // 最后挂载应用
  app.mount('#app')
})()
