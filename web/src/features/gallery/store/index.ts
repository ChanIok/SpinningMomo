import { defineStore } from 'pinia'
import { useStorage } from '@vueuse/core'
import { computed } from 'vue'
import {
  applySettingsDefaults,
  createDefaultGallerySettings,
  GALLERY_SETTINGS_STORAGE_KEY,
} from './persistence'
import { createQuerySlice } from './querySlice'
import { createNavigationSlice } from './navigationSlice'
import { createViewSlice } from './viewSlice'
import { createInteractionSlice } from './interactionSlice'
import { createLayoutSlice } from './layoutSlice'
import { createViewportSlice } from './viewportSlice'
import { createUiSlice } from './uiSlice'

/**
 * Gallery Pinia Store
 *
 * 数据流设计:
 * - Store 是单一数据来源，组件应直接从这里读取状态
 * - Composable 只负责协调 API 调用和调用 Store Actions
 *
 * 拆分说明:
 * - index.ts 只负责“装配”，不承载具体业务细节
 * - query/navigation/view/layout/viewport/ui/interaction slices 负责各自领域状态
 * - 对外仍然暴露同一个 store，避免调用方心智负担上升
 */
export const useGalleryStore = defineStore('gallery', () => {
  // 持久化数据集中在 gallerySettings，各 slice 只提供同一对象的领域访问。
  const gallerySettings = useStorage(
    GALLERY_SETTINGS_STORAGE_KEY,
    createDefaultGallerySettings(),
    localStorage,
    { mergeDefaults: (stored, defaults) => applySettingsDefaults(stored, defaults) }
  )

  // 查询与缓存层：负责结果集、分页、时间线、并发刷新版本。
  const querySlice = createQuerySlice()
  // 导航与筛选层：负责 folder/tag 树、展开态、排序筛选。
  const navigationSlice = createNavigationSlice({
    settings: gallerySettings,
  })
  // 窗口级响应式状态不写入持久化设置；页面只负责同步窗口宽度，组件直接读取这里的派生值。
  const viewportSlice = createViewportSlice()
  // 页面级共享 UI：上下文菜单、操作对话框和标签剪贴板不再由 composable 隐式单例持有。
  const uiSlice = createUiSlice()
  // 视图配置直接映射到持久化对象，避免运行时状态与持久化配置分叉。
  const viewSlice = createViewSlice({
    settings: gallerySettings,
    isCompactWindow: viewportSlice.isCompactWindow,
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
    viewSlice.resetViewState()
    layoutSlice.resetLayoutState()
    interactionSlice.resetInteractionState()
    viewportSlice.resetViewportState()
    uiSlice.resetUiState()
  }

  return {
    // 展开顺序代表心智顺序：查询、导航筛选、视图、布局，最后是交互 UI。
    ...querySlice,
    ...navigationSlice,
    ...viewSlice,
    ...layoutSlice,
    ...viewportSlice,
    ...uiSlice,
    ...interactionSlice,
    gallerySettings,
    isTimelineMode,
    reset,
  }
})
