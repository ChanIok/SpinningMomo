import { computed, ref, watch, type ComputedRef } from 'vue'
import { createGlobalState, useMediaQuery } from '@vueuse/core'
import { WIDE_VIEWPORT_QUERY } from '@/core/viewport'

// 应用级状态不随页面卸载销毁，确保 CSS 与命令式转场始终跟随视口变化。
const useMotionState = createGlobalState(() => {
  const enabledByUser = ref(true)
  const isWideViewport = useMediaQuery(WIDE_VIEWPORT_QUERY)
  // 紧凑布局保留动效，宽屏遵循用户设置；所有调用点只消费这一生效结果。
  const animationsEnabled = computed(() => !isWideViewport.value || enabledByUser.value)

  watch(
    animationsEnabled,
    (enabled) => {
      if (typeof document === 'undefined') return
      document.documentElement.dataset.motion = enabled ? 'on' : 'off'
    },
    { immediate: true, flush: 'sync' }
  )

  return { enabledByUser, animationsEnabled }
})

/** 由外观应用入口传入用户偏好，支持设置同步与外观预览。 */
export const applyAnimationsPreference = (enabled: boolean): void => {
  useMotionState().enabledByUser.value = enabled
}

/** Vue 组件与命令式调用共享同一个只读生效状态。 */
export const useAnimationsEnabled = (): ComputedRef<boolean> => useMotionState().animationsEnabled

export const shouldDisableMotion = (): boolean => !useAnimationsEnabled().value
