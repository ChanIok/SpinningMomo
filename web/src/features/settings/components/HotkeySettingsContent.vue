<script setup lang="ts">
import { useSettingsStore } from '../store'
import { useGeneralActions } from '../composables/useGeneralActions'
import { storeToRefs } from 'pinia'
import { Button } from '@/components/ui/button'
import { RotateCcw } from 'lucide-vue-next'
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
  updateFloatingWindowHotkey,
  updateScreenshotHotkey,
  updateRecordingHotkey,
  resetHotkeySettings,
} = useGeneralActions()
const { clearError } = store
const { t } = useI18n()

type HotkeyType = 'floatingWindow' | 'screenshot' | 'recording'

const getHotkey = (type: HotkeyType) => {
  const hotkey = appSettings.value.app.hotkey[type]
  return {
    modifiers: hotkey.modifiers,
    key: hotkey.key,
  }
}

const handleReset = async () => {
  await resetHotkeySettings()
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.hotkeys.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.hotkeys.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.hotkeys.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <div class="space-y-8">
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
          <Item variant="surface" size="sm">
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

          <Item variant="surface" size="sm">
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

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.general.hotkey.recording') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.general.hotkey.recordingDescription') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <HotkeyRecorder
                :value="getHotkey('recording')"
                @change="(v) => updateRecordingHotkey(v.modifiers, v.key)"
                class="w-48"
              />
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>

      <!-- Bottom Reset Section -->
      <div class="mt-12 flex justify-center pb-4">
        <ResetSettingsDialog
          :title="t('settings.hotkeys.reset.title')"
          :description="t('settings.hotkeys.reset.description')"
          @reset="handleReset"
        >
          <template #trigger>
            <Button
              variant="ghost"
              size="sm"
              class="gap-1.5 text-xs text-muted-foreground/60 transition-colors hover:bg-destructive/10 hover:text-destructive"
            >
              <RotateCcw class="h-3.5 w-3.5" />
              {{ t('settings.hotkeys.reset.title') }}
            </Button>
          </template>
        </ResetSettingsDialog>
      </div>
    </div>
  </div>
</template>
