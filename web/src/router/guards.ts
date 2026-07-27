import type { Router } from 'vue-router'
import { useSettingsStore } from '@/features/settings/store'
import { CURRENT_ONBOARDING_FLOW_VERSION } from '@/features/settings/types'

/**
 * 路由守卫配置
 */

// 全局前置守卫
export function setupRouterGuards(router: Router) {
  router.beforeEach((to) => {
    const settingsStore = useSettingsStore()
    if (settingsStore.isInitialized) {
      const onboarding = settingsStore.appSettings.app.onboarding
      const needsOnboarding =
        !onboarding.completed || onboarding.flowVersion < CURRENT_ONBOARDING_FLOW_VERSION

      if (needsOnboarding && to.name !== 'welcome') {
        return { name: 'welcome', replace: true }
      }

      if (!needsOnboarding && to.name === 'welcome') {
        return { name: 'home', replace: true }
      }
    }

    // 设置页面标题
    if (to.meta?.title) {
      document.title = `${to.meta.title} - SpinningMomo`
    } else {
      document.title = 'SpinningMomo'
    }

    // 这里可以添加权限验证、登录状态检查等逻辑
    // 例如：
    // if (to.meta.requiresAuth && !isAuthenticated()) {
    //   return '/login'
    // }
  })

  router.afterEach((to, from) => {
    // 路由切换后的逻辑，如埋点统计等
    console.log(`导航从 ${from.path} 到 ${to.path}`)
  })

  router.onError((error) => {
    console.error('路由错误:', error)
    // 可以在这里添加错误处理逻辑，如跳转到错误页面
  })
}
