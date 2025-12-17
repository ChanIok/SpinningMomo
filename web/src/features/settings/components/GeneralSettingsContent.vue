
<script setup lang="ts">
import { useSettingsStore } from '../store'
import { useGeneralActions } from '../composables/useGeneralActions'
import { storeToRefs } from 'pinia'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Label } from '@/components/ui/label'
import { Button } from '@/components/ui/button'
import HotkeyRecorder from './HotkeyRecorder.vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { useI18n } from '@/composables/useI18n'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const {
  updateLanguage,
  updateLoggerLevel,
  updateToggleVisibilityHotkey,
  updateScreenshotHotkey,
  resetGeneralSettings,
} = useGeneralActions()
const { clearError } = store
const { t } = useI18n()

// Ensure settings are passed safely to HotkeyRecorder
const getHotkey = (type: 'toggleVisibility' | 'screenshot') => {
  const hotkey = appSettings.value.app.hotkey[type]
  return {
    modifiers: hotkey.modifiers,
    key: hotkey.key
  }
}

const handleReset = async () => {
  await resetGeneralSettings()
  // simple visual feedback handled by dialog
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <!-- Header -->
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.general.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.general.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.reset.title')"
        :description="t('settings.reset.description')"
        @reset="handleReset"
      />
    </div>

    <div class="space-y-8">
      <!-- Language -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.general.language.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.general.language.description') }}
          </p>
        </div>
        
        <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
          <div class="flex items-center justify-between py-2">
            <div class="flex-1 pr-4">
              <Label class="text-sm font-medium text-foreground">
                {{ t('settings.general.language.displayLanguage') }}
              </Label>
              <p class="mt-1 text-sm text-muted-foreground">
                {{ t('settings.general.language.displayLanguageDescription') }}
              </p>
            </div>
            <div class="w-48 flex-shrink-0">
              <Select
                :model-value="appSettings.app.language.current"
                @update:model-value="(v) => updateLanguage(v as string)"
              >
                <SelectTrigger>
                  <SelectValue :placeholder="t('settings.general.language.displayLanguage')" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="zh-CN">{{ t('common.languageZhCn') }}</SelectItem>
                  <SelectItem value="en-US">{{ t('common.languageEnUs') }}</SelectItem>
                </SelectContent>
              </Select>
            </div>
          </div>
        </div>
      </div>

      <!-- Logger -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.general.logger.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.general.logger.description') }}
          </p>
        </div>
        
        <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
          <div class="flex items-center justify-between py-2">
            <div class="flex-1 pr-4">
              <Label class="text-sm font-medium text-foreground">
                {{ t('settings.general.logger.level') }}
              </Label>
              <p class="mt-1 text-sm text-muted-foreground">
                {{ t('settings.general.logger.levelDescription') }}
              </p>
            </div>
            <div class="w-48 flex-shrink-0">
              <Select
                :model-value="appSettings.app.logger.level"
                @update:model-value="(v) => updateLoggerLevel(v as string)"
              >
                <SelectTrigger>
                  <SelectValue :placeholder="t('settings.general.logger.level')" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="DEBUG">DEBUG</SelectItem>
                  <SelectItem value="INFO">INFO</SelectItem>
                  <SelectItem value="ERROR">ERROR</SelectItem>
                </SelectContent>
              </Select>
            </div>
          </div>
        </div>
      </div>

      <!-- Hotkeys -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.general.hotkey.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.general.hotkey.description') }}
          </p>
        </div>
        
        <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
          <div class="flex items-center justify-between py-2">
            <div class="flex-1 pr-4">
              <Label class="text-sm font-medium text-foreground">
                {{ t('settings.general.hotkey.toggleVisibility') }}
              </Label>
              <p class="mt-1 text-sm text-muted-foreground">
                {{ t('settings.general.hotkey.toggleVisibilityDescription') }}
              </p>
            </div>
            <div class="w-48 flex-shrink-0">
              <HotkeyRecorder
                :value="getHotkey('toggleVisibility')"
                @change="(v) => updateToggleVisibilityHotkey(v.modifiers, v.key)"
              />
            </div>
          </div>

          <div class="flex items-center justify-between py-2 mt-4 border-t pt-4">
            <div class="flex-1 pr-4">
              <Label class="text-sm font-medium text-foreground">
                {{ t('settings.general.hotkey.screenshot') }}
              </Label>
              <p class="mt-1 text-sm text-muted-foreground">
                {{ t('settings.general.hotkey.screenshotDescription') }}
              </p>
            </div>
            <div class="w-48 flex-shrink-0">
              <HotkeyRecorder
                :value="getHotkey('screenshot')"
                @change="(v) => updateScreenshotHotkey(v.modifiers, v.key)"
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
