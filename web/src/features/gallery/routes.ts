import type { RouteRecordRaw } from 'vue-router'

// Gallery 路由配置（与根路由共用，见 web/src/router/index.ts）
export default [
  {
    path: '/gallery',
    name: 'gallery',
    component: () => import('./pages/GalleryPage.vue'),
    meta: {
      title: '图库',
      icon: 'gallery',
      requiresAuth: false,
    },
  },
] as RouteRecordRaw[]
