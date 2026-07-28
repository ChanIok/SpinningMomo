import { createRouter, createWebHashHistory } from 'vue-router'
import type { RouteRecordRaw } from 'vue-router'

// 懒加载页面组件
const HomePage = () => import('@/features/home/pages/HomePage.vue')
const OnboardingPage = () => import('@/features/onboarding/pages/OnboardingPage.vue')
const SettingsPage = () => import('@/features/settings/pages/SettingsPage.vue')
const AboutPage = () => import('@/features/about/pages/AboutPage.vue')
const MapPage = () => import('@/features/map/pages/MapPage.vue')

import playgroundRoutes from '@/features/playground/routes'
import galleryRoutes from '@/features/gallery/routes'
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
      titleKey: 'app.navigation.home',
    },
  },
  {
    path: '/welcome',
    name: 'welcome',
    component: OnboardingPage,
    meta: {
      titleKey: 'app.navigation.welcome',
    },
  },
  ...galleryRoutes,
  {
    path: '/map',
    name: 'map',
    component: MapPage,
    meta: {
      titleKey: 'app.navigation.map',
    },
  },
  {
    path: '/settings',
    name: 'settings',
    component: SettingsPage,
    meta: {
      titleKey: 'app.navigation.settings',
    },
  },
  {
    path: '/about',
    name: 'about',
    component: AboutPage,
    meta: {
      titleKey: 'app.navigation.about',
    },
  },
  ...playgroundRoutes,
  {
    path: '/:pathMatch(.*)*',
    name: 'not-found',
    component: NotFoundPage,
    meta: {
      titleKey: 'app.navigation.notFound',
    },
  },
]

// 创建路由实例
const router = createRouter({
  history: createWebHashHistory(),
  routes,
})

export default router
