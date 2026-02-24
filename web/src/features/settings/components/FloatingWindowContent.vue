<script setup lang="ts">
import { watch } from 'vue'
import { useSettingsStore } from '../store'
import { useMenuActions } from '../composables/useMenuActions'
import { useAppearanceActions } from '../composables/useAppearanceActions'
import { storeToRefs } from 'pinia'
import DraggableSettingsList from './DraggableSettingsList.vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
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
import { useI18n } from '@/composables/useI18n'
import type { FloatingWindowLayout, FloatingWindowThemeMode } from '../types'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { clearError } = store
const { t } = useI18n()

const {
  featureItems,
  featureDescriptorMap,
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
  handleResetSettings: resetFloatingMenuSettings,
} = useMenuActions()

const { updateFloatingWindowLayout, updateFloatingWindowTheme, resetFloatingWindowSettings } =
  useAppearanceActions()

watch(
  isInitialized,
  (initialized) => {
    if (initialized) {
      loadFeatureItems()
    }
  },
  { immediate: true }
)

const floatingWindowThemeOptions = [
  { value: 'light', label: t('settings.appearance.floatingWindowTheme.light') },
  { value: 'dark', label: t('settings.appearance.floatingWindowTheme.dark') },
]

const getFeatureItemLabel = (id: string): string => {
  const descriptor = featureDescriptorMap.value.get(id)
  if (descriptor?.i18nKey) {
    const translated = t(descriptor.i18nKey)
    if (translated && translated !== descriptor.i18nKey) {
      return translated
    }
  }
  return id
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

const handleLayoutChange = async (field: keyof FloatingWindowLayout, value: string) => {
  const numValue = parseInt(value, 10)
  if (!isNaN(numValue) && numValue >= 0 && appSettings.value.ui.floatingWindowLayout) {
    try {
      await updateFloatingWindowLayout({
        ...appSettings.value.ui.floatingWindowLayout,
        [field]: numValue,
      })
    } catch (error) {
      console.error('Failed to update layout settings:', error)
    }
  }
}

const handleKeyDown = (e: KeyboardEvent) => {
  if (e.key === 'Enter') {
    ;(e.target as HTMLInputElement).blur()
  }
}

const handleResetSettings = async () => {
  await resetFloatingMenuSettings()
  await resetFloatingWindowSettings()
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.floatingWindow.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.floatingWindow.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.floatingWindow.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.floatingWindow.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.floatingWindow.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.floatingWindow.reset.title')"
        :description="t('settings.floatingWindow.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-8">
      <div class="space-y-6">
        <DraggableSettingsList
          :items="featureItems"
          :title="t('settings.menu.feature.title')"
          :description="t('settings.menu.feature.description')"
          :show-toggle="true"
          :get-label="getFeatureItemLabel"
          @reorder="handleFeatureReorder"
          @toggle="handleFeatureToggle"
        />

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

      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.floatingWindow.theme.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.floatingWindow.theme.description') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.appearance.floatingWindowTheme.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.appearance.floatingWindowTheme.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings?.ui?.floatingWindowThemeMode || 'dark'"
              @update:model-value="(v) => updateFloatingWindowTheme(v as FloatingWindowThemeMode)"
            >
              <SelectTrigger class="w-32">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem
                  v-for="option in floatingWindowThemeOptions"
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

      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.appearance.layout.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.appearance.layout.description') }}
          </p>
        </div>

        <ItemGroup>
          <Item
            v-for="key in [
              'baseItemHeight',
              'baseTitleHeight',
              'baseSeparatorHeight',
              'baseFontSize',
              'baseTextPadding',
              'baseIndicatorWidth',
              'baseRatioIndicatorWidth',
              'baseRatioColumnWidth',
              'baseResolutionColumnWidth',
              'baseSettingsColumnWidth',
            ] as const"
            :key="key"
            variant="surface"
            size="sm"
          >
            <ItemContent>
              <ItemTitle>
                {{ t(`settings.appearance.layout.${key}.label`) }}
              </ItemTitle>
              <ItemDescription>
                {{ t(`settings.appearance.layout.${key}.description`) }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex items-center gap-2">
                <Input
                  type="number"
                  :model-value="appSettings?.ui?.floatingWindowLayout?.[key]"
                  @input="
                    (e: Event) =>
                      handleLayoutChange(
                        key as keyof FloatingWindowLayout,
                        (e.target as HTMLInputElement).value
                      )
                  "
                  @keydown="handleKeyDown"
                  class="w-24"
                  min="0"
                />
                <span class="text-sm text-muted-foreground">
                  {{ t('settings.appearance.layout.unit') }}
                </span>
              </div>
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>
    </div>
  </div>
</template>
