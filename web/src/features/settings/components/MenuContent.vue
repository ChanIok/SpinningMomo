<script setup lang="ts">
import { useSettingsStore } from '../store'
import { useMenuActions } from '../composables/useMenuActions'
import { storeToRefs } from 'pinia'
import DraggableSettingsList from './DraggableSettingsList.vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { Button } from '@/components/ui/button'
import { useI18n } from '@/composables/useI18n'
import type { MenuItem } from '../types'
import { computed } from 'vue'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { updateFeatureItems, updateAspectRatios, updateResolutions, resetMenuSettings } =
  useMenuActions()
const { clearError } = store
const { t } = useI18n()

// 直接从 store 中获取功能项（已包含顺序和启用状态）
const getFeatureItems = computed((): MenuItem[] => {
  return appSettings.value?.ui?.appMenu?.features || []
})

const getAspectRatios = computed((): MenuItem[] => {
  const ids = appSettings.value?.ui?.appMenu?.aspectRatios || []
  return ids.map((id) => ({ id, enabled: true }))
})

const getResolutions = computed((): MenuItem[] => {
  const ids = appSettings.value?.ui?.appMenu?.resolutions || []
  return ids.map((id) => ({ id, enabled: true }))
})

// Feature Label Helper
const getFeatureItemLabel = (id: string): string => {
  const labelMap: Record<string, string> = {
    'screenshot.capture': t('settings.menu.items.screenshotCapture'),
    'screenshot.open_folder': t('settings.menu.items.screenshotOpenFolder'),
    'feature.toggle_preview': t('settings.menu.items.featureTogglePreview'),
    'feature.toggle_overlay': t('settings.menu.items.featureToggleOverlay'),
    'feature.toggle_letterbox': t('settings.menu.items.featureToggleLetterbox'),
    'feature.toggle_recording': t('settings.menu.items.featureToggleRecording'),
    'window.reset_transform': t('settings.menu.items.windowResetTransform'),
    'panel.hide': t('settings.menu.items.panelHide'),
    'app.exit': t('settings.menu.items.appExit'),
  }
  return labelMap[id] || id
}

const validateAspectRatio = (value: string): boolean => {
  const regex = /^\d+:\d+$/
  return regex.test(value) && !value.includes('0:') && !value.includes(':0')
}

const validateResolution = (value: string): boolean => {
  const resolutionRegex = /^\d+x\d+$/
  const presetRegex = /^\d+[KkPp]?$/
  return resolutionRegex.test(value) || presetRegex.test(value)
}

const handleAspectRatioAdd = async (newItem: { id: string; enabled: boolean }) => {
  const currentItems = getAspectRatios.value
  await updateAspectRatios([...currentItems, { id: newItem.id, enabled: true }])
}

const handleResolutionAdd = async (newItem: { id: string; enabled: boolean }) => {
  const currentItems = getResolutions.value
  await updateResolutions([...currentItems, { id: newItem.id, enabled: true }])
}

const handleFeatureToggle = async (id: string, enabled: boolean) => {
  const items = getFeatureItems.value.map((item) => (item.id === id ? { ...item, enabled } : item))
  await updateFeatureItems(items)
}

const handleAspectRatioRemove = async (id: string) => {
  const items = getAspectRatios.value.filter((item) => item.id !== id)
  await updateAspectRatios(items)
}

const handleResolutionRemove = async (id: string) => {
  const items = getResolutions.value.filter((item) => item.id !== id)
  await updateResolutions(items)
}

const handleResetSettings = async () => {
  await resetMenuSettings()
  // TODO: toast success
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.menu.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.menu.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.menu.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <!-- Header -->
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.menu.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.menu.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.menu.reset.title')"
        :description="t('settings.menu.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-6">
      <!-- 功能项目：全宽显示 -->
      <DraggableSettingsList
        :items="getFeatureItems"
        :title="t('settings.menu.feature.title')"
        :description="t('settings.menu.feature.description')"
        :show-toggle="true"
        :get-label="getFeatureItemLabel"
        @reorder="updateFeatureItems"
        @toggle="handleFeatureToggle"
      />

      <!-- 比例预设 + 分辨率预设：两列网格布局 -->
      <div class="grid grid-cols-1 gap-4 md:grid-cols-2">
        <DraggableSettingsList
          :items="getAspectRatios"
          :title="t('settings.menu.aspectRatio.title')"
          :description="t('settings.menu.aspectRatio.description')"
          :allow-add="true"
          :allow-remove="true"
          :show-toggle="false"
          :add-placeholder="t('settings.menu.aspectRatio.placeholder')"
          :validate-input="validateAspectRatio"
          @reorder="updateAspectRatios"
          @add="handleAspectRatioAdd"
          @remove="handleAspectRatioRemove"
        />

        <DraggableSettingsList
          :items="getResolutions"
          :title="t('settings.menu.resolution.title')"
          :description="t('settings.menu.resolution.description')"
          :allow-add="true"
          :allow-remove="true"
          :show-toggle="false"
          :add-placeholder="t('settings.menu.resolution.placeholder')"
          :validate-input="validateResolution"
          @reorder="updateResolutions"
          @add="handleResolutionAdd"
          @remove="handleResolutionRemove"
        />
      </div>
    </div>
  </div>
</template>
