<script setup lang="ts">
import { useSettingsStore } from '../store'
import { useAppearanceActions } from '../composables/useAppearanceActions'
import { useTheme } from '../composables/useTheme'
import { storeToRefs } from 'pinia'
import { Button } from '@/components/ui/button'
import { Slider } from '@/components/ui/slider'
import {
  Item,
  ItemContent,
  ItemTitle,
  ItemDescription,
  ItemActions,
  ItemGroup,
} from '@/components/ui/item'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { useI18n } from '@/composables/useI18n'
import type { WebThemeMode } from '../types'
import { SURFACE_BLUR_RANGE, SURFACE_OPACITY_RANGE } from '../constants'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { setTheme } = useTheme()
const {
  resetWebAppearanceSettings,
  updateSurfaceOpacity,
  updateSurfaceBlur,
  handleBackgroundImageSelect,
  handleBackgroundImageRemove,
} = useAppearanceActions()
const { clearError } = store
const { t } = useI18n()

const themeOptions = [
  { value: 'light', label: t('settings.appearance.theme.light') },
  { value: 'dark', label: t('settings.appearance.theme.dark') },
  { value: 'system', label: t('settings.appearance.theme.system') },
]

const handleSurfaceOpacityChange = async (val: number[] | undefined) => {
  if (!val || val.length === 0) return
  try {
    await updateSurfaceOpacity(val[0]!)
  } catch (error) {
    console.error('Failed to update surface opacity:', error)
  }
}

const handleSurfaceBlurAmountChange = async (val: number[] | undefined) => {
  if (!val || val.length === 0) return
  try {
    await updateSurfaceBlur(val[0]!)
  } catch (error) {
    console.error('Failed to update surface blur amount:', error)
  }
}

const handleThemeChange = async (themeMode: string) => {
  try {
    await setTheme(themeMode as WebThemeMode)
  } catch (error) {
    console.error('Failed to update theme:', error)
  }
}

const handleResetSettings = async () => {
  await resetWebAppearanceSettings()
}

const handleClearError = () => {
  clearError()
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.webAppearance.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.webAppearance.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="handleClearError" class="mt-2">
        {{ t('settings.webAppearance.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.webAppearance.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.webAppearance.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.webAppearance.reset.title')"
        :description="t('settings.webAppearance.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-8">
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.appearance.background.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.appearance.background.description') }}
          </p>
        </div>

        <ItemGroup>
          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.appearance.background.surfaceOpacity.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.appearance.background.surfaceOpacity.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex items-center gap-2">
                <Slider
                  :model-value="[appSettings.ui.background.surfaceOpacity]"
                  @update:model-value="handleSurfaceOpacityChange"
                  :min="SURFACE_OPACITY_RANGE.MIN"
                  :max="SURFACE_OPACITY_RANGE.MAX"
                  :step="SURFACE_OPACITY_RANGE.STEP"
                  class="w-36"
                />
                <span class="w-12 text-sm text-muted-foreground">
                  {{ (appSettings.ui.background.surfaceOpacity * 100).toFixed(0) }}%
                </span>
              </div>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.appearance.background.surfaceBlurAmount.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.appearance.background.surfaceBlurAmount.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex items-center gap-2">
                <Slider
                  :model-value="[appSettings.ui.background.surfaceBlurAmount]"
                  @update:model-value="handleSurfaceBlurAmountChange"
                  :min="SURFACE_BLUR_RANGE.MIN"
                  :max="SURFACE_BLUR_RANGE.MAX"
                  :step="SURFACE_BLUR_RANGE.STEP"
                  class="w-36"
                />
                <span class="w-12 text-sm text-muted-foreground">
                  {{ appSettings.ui.background.surfaceBlurAmount }}px
                </span>
              </div>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.appearance.background.image.label') }}
              </ItemTitle>
            </ItemContent>
            <ItemActions>
              <Button variant="outline" size="sm" @click="handleBackgroundImageSelect">
                {{ t('settings.appearance.background.image.selectButton') }}
              </Button>
              <Button
                variant="outline"
                size="sm"
                @click="handleBackgroundImageRemove"
                :disabled="appSettings.ui?.background?.type === 'none'"
              >
                {{ t('settings.appearance.background.image.removeButton') }}
              </Button>
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>

      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.appearance.theme.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.appearance.theme.description') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.appearance.theme.mode.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.appearance.theme.mode.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings.ui.webTheme.mode"
              @update:model-value="(v) => handleThemeChange(v as string)"
            >
              <SelectTrigger class="w-32">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem
                  v-for="option in themeOptions"
                  :key="option.value"
                  :value="option.value"
                >
                  {{ option.label }}
                </SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>
      </div>
    </div>
  </div>
</template>
