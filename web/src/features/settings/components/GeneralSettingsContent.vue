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
import { Button } from '@/components/ui/button'
import {
  Item,
  ItemContent,
  ItemTitle,
  ItemDescription,
  ItemActions,
  ItemGroup,
} from '@/components/ui/item'
import HotkeyRecorder from './HotkeyRecorder.vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { useI18n } from '@/composables/useI18n'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const {
  updateLanguage,
  updateLoggerLevel,
  updateFloatingWindowHotkey,
  updateScreenshotHotkey,
  resetGeneralSettings,
} = useGeneralActions()
const { clearError } = store
const { t } = useI18n()

// Ensure settings are passed safely to HotkeyRecorder
const getHotkey = (type: 'floatingWindow' | 'screenshot') => {
  const hotkey = appSettings.value.app.hotkey[type]
  return {
    modifiers: hotkey.modifiers,
    key: hotkey.key,
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

        <Item variant="outline" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.general.language.displayLanguage') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.general.language.displayLanguageDescription') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings.app.language.current"
              @update:model-value="(v) => updateLanguage(v as string)"
            >
              <SelectTrigger class="w-48">
                <SelectValue :placeholder="t('settings.general.language.displayLanguage')" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="zh-CN">{{ t('common.languageZhCn') }}</SelectItem>
                <SelectItem value="en-US">{{ t('common.languageEnUs') }}</SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>
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

        <Item variant="outline" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.general.logger.level') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.general.logger.levelDescription') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings.app.logger.level"
              @update:model-value="(v) => updateLoggerLevel(v as string)"
            >
              <SelectTrigger class="w-48">
                <SelectValue :placeholder="t('settings.general.logger.level')" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="DEBUG">DEBUG</SelectItem>
                <SelectItem value="INFO">INFO</SelectItem>
                <SelectItem value="ERROR">ERROR</SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>
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

        <ItemGroup>
          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.general.hotkey.floatingWindow') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.general.hotkey.floatingWindowDescription') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <HotkeyRecorder
                :value="getHotkey('floatingWindow')"
                @change="(v) => updateFloatingWindowHotkey(v.modifiers, v.key)"
                class="w-48"
              />
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.general.hotkey.screenshot') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.general.hotkey.screenshotDescription') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <HotkeyRecorder
                :value="getHotkey('screenshot')"
                @change="(v) => updateScreenshotHotkey(v.modifiers, v.key)"
                class="w-48"
              />
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>
    </div>
  </div>
</template>
