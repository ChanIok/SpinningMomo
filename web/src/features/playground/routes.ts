import type { RouteRecordRaw } from 'vue-router'

const routes: RouteRecordRaw[] = [
  {
    path: '/playground',
    name: 'playground',
    component: () => import('./pages/PlaygroundPage.vue'),
    meta: {
      title: '开发工具',
    },
  },
]

export default routes
