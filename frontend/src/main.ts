import { createApp } from 'vue'
import { createPinia } from 'pinia'
import naive from 'naive-ui'
import App from './App.vue'
import router from './router'
import './style.css'

const app = createApp(App)

// 先安装所有插件
app.use(createPinia())
app.use(router)
app.use(naive)

// 最后才挂载应用
app.mount('#app')
