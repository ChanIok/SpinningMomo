import { h } from 'vue'
import DefaultTheme from 'vitepress/theme'
import './custom.css'

export default {
  extends: DefaultTheme,
  Layout: () => {
    return h(DefaultTheme.Layout, null, {
      // 如果需要自定义布局，可以在这里添加
    })
  },
  enhanceApp({ app }) {
    // 注册组件等
  }
} 