import { createApp, vaporInteropPlugin } from 'vue'
import './index.css'
import App from './App.vue'

createApp(App).use(vaporInteropPlugin).mount('#app')
