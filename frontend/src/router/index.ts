import { createRouter, createWebHistory } from 'vue-router';
import MainLayout from '@/layouts/MainLayout.vue';
import ScreenshotBrowser from '@/views/screenshot/ScreenshotBrowser.vue';

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      component: MainLayout,
      children: [
        {
          path: '',
          redirect: '/screenshots'
        },
        {
          path: 'screenshots',
          name: 'screenshots',
          component: ScreenshotBrowser
        },
        {
          path: 'albums',
          name: 'albums',
          component: () => import('@/views/screenshot/ScreenshotBrowser.vue')
        }
      ]
    }
  ]
});

export default router; 