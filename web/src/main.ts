import { createApp } from 'vue'
import { watch } from 'vue'
import { createPinia } from 'pinia'
import router from './router'
import { setupDocumentTitle, setupRouterGuards } from './router/guards'
import { initializeRPC } from '@/core/rpc'
import { initializeAccessLevel } from '@/core/access'
import { initI18n } from '@/core/i18n'
import { useSettingsStore } from '@/features/settings/store'
import { CURRENT_ONBOARDING_FLOW_VERSION } from '@/features/settings/types'
import { applyAppearanceToDocument } from '@/features/settings/appearance'
import { useTaskStore } from '@/core/tasks/store'
import './index.css'
import App from './App.vue'

// 创建 Pinia 实例
const pinia = createPinia()

const app = createApp(App)

// Pinia 必须先安装，后续权限探测和路由守卫会读取其状态。
app.use(pinia)

// 初始化 RPC 通信
initializeRPC()

// 初始化应用
;(async () => {
  // 首先初始化 i18n（使用默认语言）
  await initI18n('zh-CN')

  // 先确定调用者访问等级，再初始化会根据权限显示不同内容的页面。
  await initializeAccessLevel()

  // 守卫必须在安装 Router 前注册；app.use(router) 会立即启动首次导航。
  setupRouterGuards(router)
  setupDocumentTitle(router)
  app.use(router)
  await router.isReady()

  // 然后初始化 settings store，它会自动同步后端的语言设置
  const settingsStore = useSettingsStore()
  await settingsStore.init()

  // 初始化后台任务订阅
  const taskStore = useTaskStore()
  await taskStore.initialize()

  const onboarding = settingsStore.appSettings.app.onboarding
  const needsOnboarding =
    !onboarding.completed || onboarding.flowVersion < CURRENT_ONBOARDING_FLOW_VERSION
  if (needsOnboarding && router.currentRoute.value.name !== 'welcome') {
    await router.replace('/welcome')
  }

  // 在挂载前应用主题和背景，避免首屏闪烁
  applyAppearanceToDocument(settingsStore.appSettings)

  // 监听设置变化，实时同步外观
  watch(
    () => [
      settingsStore.appSettings.ui.webTheme.mode,
      settingsStore.appSettings.ui.webTheme.customCss,
      settingsStore.appSettings.ui.webTheme.menuBlur,
      settingsStore.appSettings.ui.background.type,
      settingsStore.appSettings.ui.background.imageFileName,
      settingsStore.appSettings.ui.background.backgroundBlurAmount,
      settingsStore.appSettings.ui.background.backgroundOpacity,
      settingsStore.appSettings.ui.background.overlayColors.join('|'),
      settingsStore.appSettings.ui.background.primaryColor,
      settingsStore.appSettings.ui.background.overlayOpacity,
      settingsStore.appSettings.ui.background.surfaceOpacity,
    ],
    () => {
      applyAppearanceToDocument(settingsStore.appSettings)
    }
  )

  // 最后挂载应用
  app.mount('#app')
})()
