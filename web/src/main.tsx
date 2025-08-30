import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { BrowserRouter } from 'react-router'
import './index.css'
import App from './App.tsx'
import { initializeRPC } from './lib/rpc'
import { useWebSettingsStore } from './lib/web-settings'
import { useI18nStore } from './lib/i18n'

// 应用级初始化
const initializeApp = async () => {
  let i18nCleanup: (() => void) | null = null

  try {
    // 初始化 RPC 通信
    initializeRPC()

    // 初始化 WebSettings
    await useWebSettingsStore.getState().initialize()

    // 初始化 i18n
    i18nCleanup = await useI18nStore.getState().initialize()

    console.log('✅ 应用初始化完成')
  } catch (error) {
    console.error('❌ 应用初始化失败:', error)
  }

  // 返回清理函数
  return i18nCleanup
}

// 执行初始化
initializeApp().then((i18nCleanup) => {
  createRoot(document.getElementById('root')!).render(
    <StrictMode>
      <BrowserRouter>
        <App />
      </BrowserRouter>
    </StrictMode>
  )

  // 页面卸载时清理资源
  if (typeof window !== 'undefined' && i18nCleanup) {
    window.addEventListener('beforeunload', () => {
      i18nCleanup?.()
    })
  }
})
