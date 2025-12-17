<script setup lang="ts">
import { useSettingsStore } from '../store'
import { useMenuActions } from '../composables/useMenuActions'
import { storeToRefs } from 'pinia'
import DraggableSettingsList from './DraggableSettingsList.vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { Button } from '@/components/ui/button'
import { useI18n } from '@/composables/useI18n'
import type { FeatureItem, PresetItem } from '../types'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const {
  updateFeatureItems,
  updateAspectRatios,
  updateResolutions,
  resetMenuSettings
} = useMenuActions()
const { clearError } = store
const { t } = useI18n()

// Safely access arrays
const getFeatureItems = () => appSettings.value?.ui?.appMenu?.featureItems || []
const getAspectRatios = () => appSettings.value?.ui?.appMenu?.aspectRatios || []
const getResolutions = () => appSettings.value?.ui?.appMenu?.resolutions || []

// Feature Label Helper
const getFeatureItemLabel = (id: string): string => {
  const labelMap: Record<string, string> = {
    'screenshot.capture': t('settings.menu.items.screenshotCapture'),
    'screenshot.open_folder': t('settings.menu.items.screenshotOpenFolder'),
    'feature.toggle_preview': t('settings.menu.items.featureTogglePreview'),
    'feature.toggle_overlay': t('settings.menu.items.featureToggleOverlay'),
    'feature.toggle_letterbox': t('settings.menu.items.featureToggleLetterbox'),
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

const handleAspectRatioAdd = async (newItem: Omit<PresetItem, 'order'>) => {
    const currentItems = getAspectRatios()
    const maxOrder = currentItems.length > 0 ? Math.max(...currentItems.map(item => item.order)) : 0
    // Fix: cast to correct type or ensure types are consistent
    const itemWithOrder = { ...newItem, order: maxOrder + 1 } as PresetItem
    await updateAspectRatios([...currentItems, itemWithOrder])
}

const handleResolutionAdd = async (newItem: Omit<PresetItem, 'order'>) => {
    const currentItems = getResolutions()
    const maxOrder = currentItems.length > 0 ? Math.max(...currentItems.map(item => item.order)) : 0
    const itemWithOrder = { ...newItem, order: maxOrder + 1 } as PresetItem
    await updateResolutions([...currentItems, itemWithOrder])
}

const handleFeatureToggle = async (id: string, enabled: boolean) => {
    const items = getFeatureItems().map(item => item.id === id ? { ...item, enabled } : item)
    await updateFeatureItems(items)
}

const handleAspectRatioToggle = async (id: string, enabled: boolean) => {
    const items = getAspectRatios().map(item => item.id === id ? { ...item, enabled } : item)
    await updateAspectRatios(items)
}

const handleResolutionToggle = async (id: string, enabled: boolean) => {
    const items = getResolutions().map(item => item.id === id ? { ...item, enabled } : item)
    await updateResolutions(items)
}

const handleAspectRatioRemove = async (id: string) => {
    const items = getAspectRatios().filter(item => item.id !== id)
    await updateAspectRatios(items)
}

const handleResolutionRemove = async (id: string) => {
    const items = getResolutions().filter(item => item.id !== id)
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
      <div class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"></div>
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

    <div class="space-y-8">
        <DraggableSettingsList
            :items="getFeatureItems()"
            :title="t('settings.menu.feature.title')"
            :description="t('settings.menu.feature.description')"
            :get-label="getFeatureItemLabel"
            @reorder="updateFeatureItems"
            @toggle="handleFeatureToggle"
        />

        <DraggableSettingsList
            :items="getAspectRatios()"
            :title="t('settings.menu.aspectRatio.title')"
            :description="t('settings.menu.aspectRatio.description')"
            :allow-add="true"
            :allow-remove="true"
            :add-placeholder="t('settings.menu.aspectRatio.placeholder')"
            :validate-input="validateAspectRatio"
            @reorder="updateAspectRatios"
            @toggle="handleAspectRatioToggle"
            @add="handleAspectRatioAdd"
            @remove="handleAspectRatioRemove"
        />

        <DraggableSettingsList
            :items="getResolutions()"
             :title="t('settings.menu.resolution.title')"
            :description="t('settings.menu.resolution.description')"
             :allow-add="true"
             :allow-remove="true"
             :add-placeholder="t('settings.menu.resolution.placeholder')"
             :validate-input="validateResolution"
            @reorder="updateResolutions"
            @toggle="handleResolutionToggle"
            @add="handleResolutionAdd"
            @remove="handleResolutionRemove"
        />
    </div>
  </div>
</template>
