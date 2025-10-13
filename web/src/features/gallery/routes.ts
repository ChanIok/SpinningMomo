import type { RouteRecordRaw } from 'vue-router'

// Gallery 路由配置
export default [
  {
    path: '/gallery',
    name: 'Gallery',
    component: () => import('./pages/GalleryPage.vue'),
    meta: {
      title: '图库',
      icon: 'gallery',
      requiresAuth: false, // 根据需要调整
    }
  }
] as RouteRecordRaw[]
