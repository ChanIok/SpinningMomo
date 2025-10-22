import { createRouter, createWebHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'

// 懒加载页面组件
const HomePage = () => import('@/features/home/pages/HomePage.vue')
const GalleryPage = () => import('@/features/gallery/pages/GalleryPage.vue')
const SettingsPage = () => import('@/features/settings/pages/SettingsPage.vue')
const AboutPage = () => import('@/features/about/pages/AboutPage.vue')

// 导入playground路由
import { routes as playgroundRoutes } from '@/features/playground'

// 临时的 404 页面
const NotFoundPage = {
  template:
    '<div class="flex h-full items-center justify-center"><h1 class="text-2xl font-bold">页面未找到 404</h1></div>',
}

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
  history: createWebHistory(),
  routes,
})

export default router
