<script setup lang="ts">
import { useSettingsStore } from '../store'
import { storeToRefs } from 'pinia'
import DraggableSettingsList from './DraggableSettingsList.vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { Button } from '@/components/ui/button'
import { useI18n } from '@/composables/useI18n'
import { useMenuActions } from '../composables/useMenuActions'
import { watch } from 'vue'

const store = useSettingsStore()
const { error, isInitialized } = storeToRefs(store)
const { clearError } = store
const { t } = useI18n()

// 使用 composable 管理菜单操作
const {
  featureItems,
  aspectRatios,
  resolutions,
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
} = useMenuActions()

// 当 settings 初始化完成后加载功能项
watch(
  isInitialized,
  (initialized) => {
    if (initialized) {
      loadFeatureItems()
    }
  },
  { immediate: true }
)

// Feature Label Helper
const getFeatureItemLabel = (id: string): string => {
  const labelMap: Record<string, string> = {
    'screenshot.capture': t('menu.screenshot_capture'),
    'screenshot.open_folder': t('menu.screenshot_open_folder'),
    'preview.toggle': t('menu.preview_toggle'),
    'overlay.toggle': t('menu.overlay_toggle'),
    'letterbox.toggle': t('menu.letterbox_toggle'),
    'recording.toggle': t('menu.recording_toggle'),
    'motion_photo.toggle': t('menu.motion_photo_toggle'),
    'replay_buffer.toggle': t('menu.replay_buffer_toggle'),
    'replay_buffer.save': t('menu.replay_buffer_save'),
    'window.reset': t('menu.window_reset'),
    'app.float': t('menu.app_float'),
    'app.main': t('menu.app_main'),
    'app.exit': t('menu.app_exit'),
  }
  return labelMap[id] || id
}

// UI 验证函数
const validateAspectRatio = (value: string): boolean => {
  const regex = /^\d+:\d+$/
  return regex.test(value) && !value.includes('0:') && !value.includes(':0')
}

const validateResolution = (value: string): boolean => {
  const resolutionRegex = /^\d+x\d+$/
  const presetRegex = /^\d+[KkPp]?$/
  return resolutionRegex.test(value) || presetRegex.test(value)
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
        :items="featureItems"
        :title="t('settings.menu.feature.title')"
        :description="t('settings.menu.feature.description')"
        :show-toggle="true"
        :get-label="getFeatureItemLabel"
        @reorder="handleFeatureReorder"
        @toggle="handleFeatureToggle"
      />

      <!-- 比例预设 + 分辨率预设：两列网格布局 -->
      <div class="grid grid-cols-1 gap-4 md:grid-cols-2">
        <DraggableSettingsList
          :items="aspectRatios"
          :title="t('settings.menu.aspectRatio.title')"
          :description="t('settings.menu.aspectRatio.description')"
          :allow-add="true"
          :allow-remove="true"
          :show-toggle="false"
          :add-placeholder="t('settings.menu.aspectRatio.placeholder')"
          :validate-input="validateAspectRatio"
          @reorder="handleAspectRatioReorder"
          @add="handleAspectRatioAdd"
          @remove="handleAspectRatioRemove"
        />

        <DraggableSettingsList
          :items="resolutions"
          :title="t('settings.menu.resolution.title')"
          :description="t('settings.menu.resolution.description')"
          :allow-add="true"
          :allow-remove="true"
          :show-toggle="false"
          :add-placeholder="t('settings.menu.resolution.placeholder')"
          :validate-input="validateResolution"
          @reorder="handleResolutionReorder"
          @add="handleResolutionAdd"
          @remove="handleResolutionRemove"
        />
      </div>
    </div>
  </div>
</template>
