import { createApp } from 'vue'
import { createPinia } from 'pinia'
import router from './router'
import { setupRouterGuards } from './router/guards'
import { initializeRPC } from '@/core/rpc'
import { initI18n } from '@/core/i18n'
import { useSettingsStore } from '@/features/settings/store'
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

  // 最后挂载应用
  app.mount('#app')
})()
