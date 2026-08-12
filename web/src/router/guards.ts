import { watch } from 'vue'
import type { Router } from 'vue-router'
import { useI18n } from '@/core/i18n'
import { useSettingsStore } from '@/features/settings/store'
import { CURRENT_ONBOARDING_FLOW_VERSION } from '@/features/settings/types'
import { isLocalAccess } from '@/core/access'

/**
 * 路由守卫与文档标题配置
 */

// 响应式更新 document.title
export function setupDocumentTitle(router: Router) {
  const { t, locale } = useI18n()
  watch(
    [() => router.currentRoute.value, locale],
    ([currentRoute]) => {
      const titleKey = currentRoute.meta?.titleKey as string | undefined
      const staticTitle = currentRoute.meta?.title as string | undefined
      const pageTitle = titleKey ? t(titleKey) : staticTitle

      if (pageTitle) {
        document.title = `${pageTitle} - SpinningMomo`
      } else {
        document.title = 'SpinningMomo'
      }
    },
    { immediate: true }
  )
}

// 全局前置守卫
export function setupRouterGuards(router: Router) {
  router.beforeEach((to) => {
    // 权限探测完成后，LAN 和 unknown 都不能进入宿主机设置页面。
    // 只有明确的 local 调用者可以进入宿主机设置、引导和第三方地图页面；
    // LAN 或权限未知时统一回到图库，避免展示无法执行的宿主机操作。
    if (
      !isLocalAccess() &&
      (to.name === 'settings' || to.name === 'welcome' || to.name === 'map')
    ) {
      return { name: 'gallery', replace: true }
    }

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
