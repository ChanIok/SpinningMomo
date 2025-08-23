import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { BrowserRouter } from 'react-router'
import './index.css'
import App from './App.tsx'
import { initializeRPC } from './lib/rpc'
import { initializeWebSettings } from './lib/web-settings/webSettingsApi'

// 应用级初始化
const initializeApp = async () => {
  try {
    // 初始化 RPC 通信
    initializeRPC()
    
    // 初始化 WebSettings
    await initializeWebSettings()
    console.log('✅ 应用初始化完成')
  } catch (error) {
    console.error('❌ 应用初始化失败:', error)
  }
}

// 执行初始化
initializeApp().then(() => {
  createRoot(document.getElementById('root')!).render(
    <StrictMode>
      <BrowserRouter>
        <App />
      </BrowserRouter>
    </StrictMode>
  )
})
