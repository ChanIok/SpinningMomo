import type { RouteRecordRaw } from 'vue-router'

const routes: RouteRecordRaw[] = [
  {
    path: '/playground',
    name: 'playground',
    component: () => import('./pages/ApiPlaygroundPage.vue'),
    meta: {
      title: 'API 测试工具',
    },
  },
]

export default routes
