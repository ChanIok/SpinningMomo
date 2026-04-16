import { defineStore } from 'pinia'
import { useStorage } from '@vueuse/core'
import { computed } from 'vue'
import { createDefaultGallerySettings, GALLERY_SETTINGS_STORAGE_KEY } from './persistence'
import { createQuerySlice } from './querySlice'
import { createNavigationSlice } from './navigationSlice'
import { createInteractionSlice } from './interactionSlice'
import { createLayoutSlice } from './layoutSlice'

/**
 * Gallery Pinia Store
 *
 * 数据流设计:
 * - Store 是单一数据来源，组件应直接从这里读取状态
 * - Composable 只负责协调 API 调用和调用 Store Actions
 *
 * 拆分说明:
 * - index.ts 只负责“装配”，不承载具体业务细节
 * - query/navigation/layout/interaction 四个 slice 负责各自领域状态
 * - 对外仍然暴露同一个 store，避免调用方心智负担上升
 */
export const useGalleryStore = defineStore('gallery', () => {
  // 持久化层保持单一根对象：gallerySettings 是 gallery 偏好的唯一入口。
  const gallerySettings = useStorage(
    GALLERY_SETTINGS_STORAGE_KEY,
    createDefaultGallerySettings(),
    localStorage
  )

  // 查询与缓存层：负责结果集、分页、时间线、并发刷新版本。
  const querySlice = createQuerySlice()
  // 导航与筛选层：负责 folder/tag 树、展开态、排序筛选、视图配置。
  const navigationSlice = createNavigationSlice({
    settings: gallerySettings,
  })
  // 布局层：负责三栏布局的完整真相源与本地持久化。
  const layoutSlice = createLayoutSlice({
    settings: gallerySettings,
  })
  // 交互层：负责 selection/lightbox/details focus，并依赖 query 结果做就地 patch。
  const interactionSlice = createInteractionSlice({
    totalCount: querySlice.totalCount,
    paginatedAssets: querySlice.paginatedAssets,
    bumpPaginatedAssetsVersion: () => {
      querySlice.paginatedAssetsVersion.value += 1
    },
  })

  // timeline mode 本质是按 createdAt 排序的特化表现，保持历史语义兼容。
  const isTimelineMode = computed(() => navigationSlice.sortBy.value === 'createdAt')

  // reset 只保留一个入口，避免“某个 slice 忘记重置”的问题。
  function reset() {
    querySlice.resetQueryState()
    navigationSlice.resetNavigationState()
    layoutSlice.resetLayoutState()
    interactionSlice.resetInteractionState()
  }

  return {
    // 展开顺序代表心智顺序：先数据查询，再导航筛选，再布局，最后交互 UI。
    ...querySlice,
    ...navigationSlice,
    ...layoutSlice,
    ...interactionSlice,
    gallerySettings,
    isTimelineMode,
    reset,
  }
})
