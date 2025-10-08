import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'

// 临时简单的组件，用于避免 TypeScript 错误
// TODO: 替换为实际的页面组件
const TempHome = { template: '<div>首页</div>' }
const TempGallery = { template: '<div>画廊</div>' }
const TempSettings = { template: '<div>设置</div>' }
const TempAbout = { template: '<div>关于</div>' }
const TempNotFound = { template: '<div>页面未找到</div>' }

// 基础路由配置
const routes: RouteRecordRaw[] = [
  {
    path: '/',
    name: 'home',
    component: TempHome,
    meta: {
      title: '首页'
    }
  },
  {
    path: '/gallery',
    name: 'gallery',
    component: TempGallery,
    meta: {
      title: '画廊'
    }
  },
  {
    path: '/settings',
    name: 'settings',
    component: TempSettings,
    meta: {
      title: '设置'
    }
  },
  {
    path: '/about',
    name: 'about',
    component: TempAbout,
    meta: {
      title: '关于'
    }
  },
  {
    path: '/:pathMatch(.*)*',
    name: 'not-found',
    component: TempNotFound,
    meta: {
      title: '页面未找到'
    }
  }
]

// 创建路由实例
const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router