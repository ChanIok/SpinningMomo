import { createApp } from 'vue'
import { createPinia } from 'pinia'
import router from './router'
import { setupRouterGuards } from './router/guards'
import { initializeRPC } from '@/core/rpc'
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

app.mount('#app')
