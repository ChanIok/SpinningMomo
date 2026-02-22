import { createRouter, createWebHashHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'

// 懒加载页面组件
const HomePage = () => import('@/features/home/pages/HomePage.vue')
const GalleryPage = () => import('@/features/gallery/pages/GalleryPage.vue')
const SettingsPage = () => import('@/features/settings/pages/SettingsPage.vue')
const AboutPage = () => import('@/features/about/pages/AboutPage.vue')

// 导入playground路由
import { routes as playgroundRoutes } from '@/features/playground'
import NotFoundPage from '@/features/common/pages/NotFoundPage.vue'

// 基础路由配置
const routes: RouteRecordRaw[] = [
  {
    path: '/',
    redirect: '/home',
  },
  {
    path: '/home',
    name: 'home',
    component: HomePage,
    meta: {
      title: '首页',
    },
  },
  {
    path: '/gallery',
    name: 'gallery',
    component: GalleryPage,
    meta: {
      title: '图库',
    },
  },
  {
    path: '/settings',
    name: 'settings',
    component: SettingsPage,
    meta: {
      title: '设置',
    },
  },
  {
    path: '/about',
    name: 'about',
    component: AboutPage,
    meta: {
      title: '关于',
    },
  },
  // 添加playground路由
  ...playgroundRoutes,
  {
    path: '/:pathMatch(.*)*',
    name: 'not-found',
    component: NotFoundPage,
    meta: {
      title: '页面未找到',
    },
  },
]

// 创建路由实例
const router = createRouter({
  history: createWebHashHistory(),
  routes,
})

export default router
