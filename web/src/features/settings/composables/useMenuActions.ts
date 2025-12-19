import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS } from '../types'
import type { MenuItem } from '../types'
import { storeToRefs } from 'pinia'
import { featuresApi } from '../featuresApi'
import { ref, computed } from 'vue'

export const useMenuActions = () => {
  const store = useSettingsStore()
  const { appSettings, isInitialized } = storeToRefs(store)

  // 功能项状态
  const featureItems = ref<MenuItem[]>([])
  const isLoadingFeatures = ref(false)

  // 加载功能项列表
  const loadFeatureItems = async () => {
    if (!isInitialized.value) return

    isLoadingFeatures.value = true
    try {
      // 获取所有可用功能
      const allFeatures = await featuresApi.getAll()
      const enabledFeatures = appSettings.value?.ui?.appMenu?.features || []

      // 构建启用功能的索引映射
      const enabledMap = new Map<string, number>()
      enabledFeatures.forEach((id, index) => {
        enabledMap.set(id, index)
      })

      const items: MenuItem[] = []

      // 先添加已启用的功能（按顺序）
      enabledFeatures.forEach((id, index) => {
        items.push({
          id,
          enabled: true,
          order: index,
        })
      })

      // 再添加未启用的功能
      allFeatures.forEach((feature) => {
        if (!enabledMap.has(feature.id)) {
          items.push({
            id: feature.id,
            enabled: false,
            order: -1,
          })
        }
      })

      featureItems.value = items
    } catch (e) {
      console.error('Failed to load feature items:', e)
    } finally {
      isLoadingFeatures.value = false
    }
  }

  // 通用辅助函数：将 ID 数组转换为 MenuItem 数组
  const createMenuItems = (ids: string[]): MenuItem[] => {
    return ids.map((id) => ({ id, enabled: true }))
  }

  // 计算属性：比例列表
  const aspectRatios = computed((): MenuItem[] => {
    return createMenuItems(appSettings.value?.ui?.appMenu?.aspectRatios || [])
  })

  // 计算属性：分辨率列表
  const resolutions = computed((): MenuItem[] => {
    return createMenuItems(appSettings.value?.ui?.appMenu?.resolutions || [])
  })

  // 通用辅助函数：更新 appMenu 字段
  const updateAppMenuField = async (
    field: 'features' | 'aspectRatios' | 'resolutions',
    value: string[]
  ) => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: {
          ...appSettings.value.ui.appMenu,
          [field]: value,
        },
      },
    })
  }

  // 提取启用的功能项 ID
  const getEnabledFeatureIds = (): string[] => {
    return featureItems.value.filter((item) => item.enabled).map((item) => item.id)
  }

  // === 功能项操作 ===
  const handleFeatureToggle = async (id: string, enabled: boolean) => {
    featureItems.value = featureItems.value.map((item) =>
      item.id === id ? { ...item, enabled } : item
    )
    await updateAppMenuField('features', getEnabledFeatureIds())
  }

  const handleFeatureReorder = async (items: MenuItem[]) => {
    featureItems.value = items.map((item, index) => ({
      ...item,
      order: item.enabled ? index : -1,
    }))
    await updateAppMenuField('features', getEnabledFeatureIds())
  }

  // === 比例操作 ===
  const handleAspectRatioAdd = async (newItem: { id: string; enabled: boolean }) => {
    const currentIds = appSettings.value.ui.appMenu.aspectRatios
    await updateAppMenuField('aspectRatios', [...currentIds, newItem.id])
  }

  const handleAspectRatioRemove = async (id: string) => {
    const currentIds = appSettings.value.ui.appMenu.aspectRatios
    await updateAppMenuField(
      'aspectRatios',
      currentIds.filter((ratioId) => ratioId !== id)
    )
  }

  const handleAspectRatioReorder = async (items: MenuItem[]) => {
    await updateAppMenuField(
      'aspectRatios',
      items.map((item) => item.id)
    )
  }

  // === 分辨率操作 ===
  const handleResolutionAdd = async (newItem: { id: string; enabled: boolean }) => {
    const currentIds = appSettings.value.ui.appMenu.resolutions
    await updateAppMenuField('resolutions', [...currentIds, newItem.id])
  }

  const handleResolutionRemove = async (id: string) => {
    const currentIds = appSettings.value.ui.appMenu.resolutions
    await updateAppMenuField(
      'resolutions',
      currentIds.filter((resId) => resId !== id)
    )
  }

  const handleResolutionReorder = async (items: MenuItem[]) => {
    await updateAppMenuField(
      'resolutions',
      items.map((item) => item.id)
    )
  }

  // === 重置设置 ===
  const handleResetSettings = async () => {
    await store.updateSettings({
      ...appSettings.value,
      ui: {
        ...appSettings.value.ui,
        appMenu: DEFAULT_APP_SETTINGS.ui.appMenu,
      },
    })
    await loadFeatureItems()
  }

  return {
    // 状态
    featureItems,
    isLoadingFeatures,
    aspectRatios,
    resolutions,

    // 方法
    loadFeatureItems,
    handleFeatureToggle,
    handleFeatureReorder,
    handleAspectRatioAdd,
    handleAspectRatioRemove,
    handleAspectRatioReorder,
    handleResolutionAdd,
    handleResolutionRemove,
    handleResolutionReorder,
    handleResetSettings,
  }
}
