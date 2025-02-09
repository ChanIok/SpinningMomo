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
          redirect: 'screenshots'
        },
        {
          path: 'screenshots',
          name: 'screenshots',
          component: ScreenshotBrowser
        },
        {
          path: 'albums',
          name: 'albums',
          component: () => import('@/views/album/AlbumView.vue')
        },
        {
          path: 'albums/:id',
          name: 'albumDetail',
          component: () => import('@/views/screenshot/ScreenshotBrowser.vue'),
          props: true
        },
        {
          path: 'calendar',
          name: 'Calendar',
          component: () => import('@/views/screenshot/CalendarView.vue')
        },
        {
          path: 'calendar/:year/:month',
          name: 'MonthView',
          component: () => import('@/views/screenshot/ScreenshotBrowser.vue'),
          props: true
        },
        {
          path: 'settings',
          name: 'settings',
          component: () => import('@/views/settings/SettingsView.vue')
        }
      ]
    }
  ]
});

export default router; 