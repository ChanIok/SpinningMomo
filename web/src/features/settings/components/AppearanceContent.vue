<script setup lang="ts">
import { computed } from 'vue'
import { useSettingsStore } from '../store'
import { useAppearanceActions } from '../composables/useAppearanceActions'
import { useTheme } from '../composables/useTheme'
import { storeToRefs } from 'pinia'
import { Button } from '@/components/ui/button'
import { Slider } from '@/components/ui/slider'
import { Switch } from '@/components/ui/switch'
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
import OverlayPaletteEditor from './OverlayPaletteEditor.vue'
import { useI18n } from '@/composables/useI18n'
import type { CjkFontPreset, WebThemeMode } from '../types'
import type { OverlayPalette, OverlayPalettePreset } from '../overlayPalette'
import { getOverlayPaletteFromBackground } from '../overlayPalette'
import { resolveBackgroundImageUrl } from '../backgroundPath'
import { sampleOverlayPaletteFromWallpaper } from '../overlayPaletteSampler'
import {
  SURFACE_OPACITY_RANGE,
  BACKGROUND_BLUR_RANGE,
  BACKGROUND_OPACITY_RANGE,
  OVERLAY_OPACITY_RANGE,
} from '../constants'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { setTheme } = useTheme()
const {
  resetWebAppearanceSettings,
  updateBackgroundOpacity,
  updateBackgroundBlur,
  updateOverlayOpacity,
  updateOverlayPalette,
  applyOverlayPalettePreset,
  updateWebViewTransparentBackground,
  updateCjkFontPreset,
  updateSurfaceOpacity,
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
const cjkFontOptions = [
  { value: 'harmony', label: t('settings.appearance.theme.font.harmony') },
  { value: 'microsoft', label: t('settings.appearance.theme.font.microsoft') },
]
const overlayPalette = computed<OverlayPalette>(() =>
  getOverlayPaletteFromBackground(appSettings.value.ui.background)
)
const canSampleOverlayPaletteFromWallpaper = computed(() => {
  return Boolean(resolveBackgroundImageUrl(appSettings.value.ui.background))
})

const handleSurfaceOpacityChange = async (val: number[] | undefined) => {
  if (!val || val.length === 0) return
  try {
    await updateSurfaceOpacity(val[0]!)
  } catch (error) {
    console.error('Failed to update surface opacity:', error)
  }
}

const handleBackgroundOpacityChange = async (val: number[] | undefined) => {
  if (!val || val.length === 0) return
  try {
    await updateBackgroundOpacity(val[0]!)
  } catch (error) {
    console.error('Failed to update background opacity:', error)
  }
}

const handleBackgroundBlurAmountChange = async (val: number[] | undefined) => {
  if (!val || val.length === 0) return
  try {
    await updateBackgroundBlur(val[0]!)
  } catch (error) {
    console.error('Failed to update background blur amount:', error)
  }
}

const handleOverlayOpacityChange = async (val: number[] | undefined) => {
  if (!val || val.length === 0) return
  try {
    await updateOverlayOpacity(val[0]!)
  } catch (error) {
    console.error('Failed to update overlay opacity:', error)
  }
}

const handleOverlayPaletteChange = async (palette: OverlayPalette) => {
  try {
    await updateOverlayPalette(palette)
  } catch (error) {
    console.error('Failed to update overlay palette:', error)
  }
}

const handleOverlayPresetApply = async (preset: OverlayPalettePreset) => {
  try {
    await applyOverlayPalettePreset(preset)
  } catch (error) {
    console.error('Failed to apply overlay palette preset:', error)
  }
}

const handleOverlaySampleFromWallpaper = async () => {
  const imageUrl = resolveBackgroundImageUrl(appSettings.value.ui.background)
  if (!imageUrl) return

  try {
    const nextPalette = await sampleOverlayPaletteFromWallpaper({
      imageUrl,
      mode: overlayPalette.value.mode,
      themeMode: appSettings.value.ui.webTheme.mode,
    })

    await updateOverlayPalette(nextPalette)
  } catch (error) {
    console.error('Failed to sample overlay palette from wallpaper:', error)
  }
}

const handleThemeChange = async (themeMode: string) => {
  try {
    await setTheme(themeMode as WebThemeMode)
  } catch (error) {
    console.error('Failed to update theme:', error)
  }
}

const handleCjkFontPresetChange = async (preset: string) => {
  try {
    await updateCjkFontPreset(preset as CjkFontPreset)
  } catch (error) {
    console.error('Failed to update CJK font preset:', error)
  }
}

const handleWebViewTransparentBackgroundChange = async (enabled: boolean) => {
  try {
    await updateWebViewTransparentBackground(enabled)
  } catch (error) {
    console.error('Failed to update WebView transparent background setting:', error)
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
                {{ t('settings.appearance.background.backgroundOpacity.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.appearance.background.backgroundOpacity.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex items-center gap-2">
                <Slider
                  :model-value="[appSettings.ui.background.backgroundOpacity]"
                  @update:model-value="handleBackgroundOpacityChange"
                  :min="BACKGROUND_OPACITY_RANGE.MIN"
                  :max="BACKGROUND_OPACITY_RANGE.MAX"
                  :step="BACKGROUND_OPACITY_RANGE.STEP"
                  class="w-36"
                />
                <span class="w-12 text-sm text-muted-foreground">
                  {{ (appSettings.ui.background.backgroundOpacity * 100).toFixed(0) }}%
                </span>
              </div>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.appearance.background.backgroundBlurAmount.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.appearance.background.backgroundBlurAmount.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex items-center gap-2">
                <Slider
                  :model-value="[appSettings.ui.background.backgroundBlurAmount]"
                  @update:model-value="handleBackgroundBlurAmountChange"
                  :min="BACKGROUND_BLUR_RANGE.MIN"
                  :max="BACKGROUND_BLUR_RANGE.MAX"
                  :step="BACKGROUND_BLUR_RANGE.STEP"
                  class="w-36"
                />
                <span class="w-12 text-sm text-muted-foreground">
                  {{ appSettings.ui.background.backgroundBlurAmount }}px
                </span>
              </div>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.appearance.background.overlayOpacity.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.appearance.background.overlayOpacity.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex items-center gap-2">
                <Slider
                  :model-value="[appSettings.ui.background.overlayOpacity]"
                  @update:model-value="handleOverlayOpacityChange"
                  :min="OVERLAY_OPACITY_RANGE.MIN"
                  :max="OVERLAY_OPACITY_RANGE.MAX"
                  :step="OVERLAY_OPACITY_RANGE.STEP"
                  class="w-36"
                />
                <span class="w-12 text-sm text-muted-foreground">
                  {{ (appSettings.ui.background.overlayOpacity * 100).toFixed(0) }}%
                </span>
              </div>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.appearance.background.overlayPalette.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.appearance.background.overlayPalette.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <OverlayPaletteEditor
                :model-value="overlayPalette"
                :show-wallpaper-sampler="canSampleOverlayPaletteFromWallpaper"
                @update:model-value="handleOverlayPaletteChange"
                @apply-preset="handleOverlayPresetApply"
                @sample-from-wallpaper="handleOverlaySampleFromWallpaper"
              />
            </ItemActions>
          </Item>

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

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.appearance.theme.font.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.appearance.theme.font.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings.ui.webTheme.cjkFontPreset"
              @update:model-value="(v) => handleCjkFontPresetChange(v as string)"
            >
              <SelectTrigger class="w-48">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem
                  v-for="option in cjkFontOptions"
                  :key="option.value"
                  :value="option.value"
                >
                  {{ option.label }}
                </SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.appearance.webview.transparentBackground.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.appearance.webview.transparentBackground.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Switch
              :model-value="appSettings.ui.webviewWindow.enableTransparentBackground"
              @update:model-value="
                (value) => handleWebViewTransparentBackgroundChange(Boolean(value))
              "
            />
          </ItemActions>
        </Item>
      </div>
    </div>
  </div>
</template>
