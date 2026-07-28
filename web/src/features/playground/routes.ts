import type { RouteRecordRaw } from 'vue-router'

const routes: RouteRecordRaw[] = import.meta.env.DEV
  ? [
      {
        path: '/playground',
        name: 'playground',
        component: () => import('./pages/PlaygroundPage.vue'),
        meta: {
          titleKey: 'app.navigation.playground',
        },
      },
    ]
  : []

export default routes
