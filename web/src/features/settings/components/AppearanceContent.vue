
<script setup lang="ts">
import { onMounted } from 'vue'
import { useSettingsStore } from '../store'
import { useAppearanceActions } from '../composables/useAppearanceActions'
import { useTheme } from '../composables/useTheme'
import { storeToRefs } from 'pinia'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Slider } from '@/components/ui/slider'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { useI18n } from '@/composables/useI18n'
import type { AppWindowLayout, AppWindowThemeMode, WebThemeMode } from '../types'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { setTheme } = useTheme()

const {
    updateAppWindowLayout,
    resetAppearanceSettings,
    updateBackgroundOpacity,
    updateBackgroundBlur,
    handleBackgroundImageSelect,
    handleBackgroundImageRemove,
    updateAppWindowTheme
} = useAppearanceActions()

const { clearError } = store
const { t } = useI18n()

const themeOptions = [
    { value: 'light', label: t('settings.appearance.theme.light') },
    { value: 'dark', label: t('settings.appearance.theme.dark') },
    { value: 'system', label: t('settings.appearance.theme.system') },
]

const appWindowThemeOptions = [
    { value: 'light', label: t('settings.appearance.appWindowTheme.light') },
    { value: 'dark', label: t('settings.appearance.appWindowTheme.dark') },
]

const handleLayoutChange = async (field: keyof AppWindowLayout, value: string) => {
    const numValue = parseInt(value, 10)
    if (!isNaN(numValue) && numValue >= 0 && appSettings.value.ui.appWindowLayout) {
        try {
            await updateAppWindowLayout({
                ...appSettings.value.ui.appWindowLayout,
                [field]: numValue
            })
        } catch (error) {
            console.error('Failed to update layout settings:', error)
            // TODO: toast error
        }
    }
}

const handleKeyDown = (e: KeyboardEvent) => {
    if (e.key === 'Enter') {
        (e.target as HTMLInputElement).blur()
    }
}

const handleOpacityChange = async (val: number[] | undefined) => {
    if (!val || val.length === 0) return
    try {
        await updateBackgroundOpacity(val[0]!)
    } catch (error) {
        console.error('Failed to update opacity:', error)
        // TODO: toast error
    }
}

const handleBlurAmountChange = async (val: number[] | undefined) => {
    if (!val || val.length === 0) return
    try {
        await updateBackgroundBlur(val[0]!)
    } catch (error) {
        console.error('Failed to update blur amount:', error)
        // TODO: toast error
    }
}

const handleThemeChange = async (themeMode: string) => {
    try {
        await setTheme(themeMode as WebThemeMode)
    } catch (error) {
        console.error('Failed to update theme:', error)
        // TODO: toast error
    }
}

const handleResetSettings = async () => {
    await resetAppearanceSettings()
    // TODO: toast success
}

const handleClearError = () => {
    clearError()
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.appearance.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.appearance.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="handleClearError" class="mt-2">
        {{ t('settings.appearance.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <!-- Header -->
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.appearance.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.appearance.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.appearance.reset.title')"
        :description="t('settings.appearance.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-8">
      <!-- Background Settings -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
             {{ t('settings.appearance.background.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
             {{ t('settings.appearance.background.description') }}
          </p>
        </div>

        <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
             <!-- Opacity -->
             <div class="flex items-center justify-between py-2">
                <div class="flex-1 pr-4">
                    <Label class="text-sm font-medium text-foreground">
                       {{ t('settings.appearance.background.opacity.label') }}
                    </Label>
                    <p class="mt-1 text-sm text-muted-foreground">
                       {{ t('settings.appearance.background.opacity.description') }}
                    </p>
                </div>
                <div class="flex flex-shrink-0 items-center gap-2">
                    <div class="w-36">
                        <Slider
                            :model-value="[appSettings.ui.background.opacity]"
                            @update:model-value="handleOpacityChange"
                            :min="0"
                            :max="1"
                            :step="0.1"
                            class="w-full"
                        />
                    </div>
                    <span class="w-12 text-sm text-muted-foreground">
                        {{ (appSettings.ui.background.opacity * 100).toFixed(0) }}%
                    </span>
                </div>
             </div>

             <!-- Blur -->
             <div class="flex items-center justify-between py-2">
                <div class="flex-1 pr-4">
                    <Label class="text-sm font-medium text-foreground">
                       {{ t('settings.appearance.background.blurAmount.label') }}
                    </Label>
                    <p class="mt-1 text-sm text-muted-foreground">
                       {{ t('settings.appearance.background.blurAmount.description') }}
                    </p>
                </div>
                 <div class="flex flex-shrink-0 items-center gap-2">
                    <div class="w-36">
                        <Slider
                            :model-value="[appSettings.ui.background.blurAmount]"
                            @update:model-value="handleBlurAmountChange"
                            :min="0"
                            :max="200"
                            :step="1"
                            class="w-full"
                        />
                    </div>
                    <span class="w-12 text-sm text-muted-foreground">
                        {{ appSettings.ui.background.blurAmount }}px
                    </span>
                 </div>
             </div>

             <!-- Background Image -->
             <div class="flex items-center justify-between py-2">
                <div class="flex-1 pr-4">
                   <Label class="text-sm font-medium text-foreground">
                       {{ t('settings.appearance.background.image.label') }}
                   </Label>
                </div>
                <div class="flex flex-shrink-0 gap-2">
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
                </div>
             </div>
        </div>
      </div>

      <!-- Theme Settings -->
      <div class="space-y-4">
         <div>
             <h3 class="text-lg font-semibold text-foreground">
               {{ t('settings.appearance.theme.title') }}
             </h3>
             <p class="mt-1 text-sm text-muted-foreground">
               {{ t('settings.appearance.theme.description') }}
             </p>
         </div>

         <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
             <div class="flex items-center justify-between py-2">
                 <div class="flex-1 pr-4">
                   <Label class="text-sm font-medium text-foreground">
                       {{ t('settings.appearance.theme.mode.label') }}
                   </Label>
                   <p class="mt-1 text-sm text-muted-foreground">
                       {{ t('settings.appearance.theme.mode.description') }}
                   </p>
                 </div>
                <div class="flex flex-shrink-0">
                    <Select :model-value="appSettings.ui.webTheme.mode" @update:model-value="(v) => handleThemeChange(v as string)">
                        <SelectTrigger class="w-32">
                           <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                           <SelectItem v-for="option in themeOptions" :key="option.value" :value="option.value">
                              {{ option.label }}
                           </SelectItem>
                        </SelectContent>
                    </Select>
                 </div>
             </div>

             <div class="flex items-center justify-between py-2">
                 <div class="flex-1 pr-4">
                   <Label class="text-sm font-medium text-foreground">
                       {{ t('settings.appearance.appWindowTheme.label') }}
                   </Label>
                   <p class="mt-1 text-sm text-muted-foreground">
                       {{ t('settings.appearance.appWindowTheme.description') }}
                   </p>
                 </div>
                 <div class="flex flex-shrink-0">
                    <Select
                       :model-value="appSettings?.ui?.appWindowThemeMode || 'dark'"
                       @update:model-value="(v) => updateAppWindowTheme(v as AppWindowThemeMode)"
                    >
                        <SelectTrigger class="w-32">
                           <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                           <SelectItem v-for="option in appWindowThemeOptions" :key="option.value" :value="option.value">
                              {{ option.label }}
                           </SelectItem>
                        </SelectContent>
                    </Select>
                 </div>
             </div>
         </div>
      </div>

      <!-- Layout Settings -->
      <div class="space-y-4">
         <div>
            <h3 class="text-lg font-semibold text-foreground">
               {{ t('settings.appearance.layout.title') }}
            </h3>
            <p class="mt-1 text-sm text-muted-foreground">
               {{ t('settings.appearance.layout.description') }}
            </p>
         </div>

         <div class="rounded-lg border bg-card p-4 text-card-foreground shadow-sm">
            <template v-for="(key) in ([
                'baseItemHeight', 'baseTitleHeight', 'baseSeparatorHeight', 'baseFontSize',
                'baseTextPadding', 'baseIndicatorWidth', 'baseRatioIndicatorWidth',
                'baseRatioColumnWidth', 'baseResolutionColumnWidth', 'baseSettingsColumnWidth'
            ] as const)" :key="key">
                <div class="flex items-center justify-between py-2">
                   <div class="flex-1 pr-4">
                       <Label class="text-sm font-medium text-foreground">
                           {{ t(`settings.appearance.layout.${key}.label`) }}
                       </Label>
                       <p class="mt-1 text-sm text-muted-foreground">
                           {{ t(`settings.appearance.layout.${key}.description`) }}
                       </p>
                   </div>
                   <div class="flex flex-shrink-0 items-center gap-2">
                       <Input
                           type="number"
                           :model-value="appSettings?.ui?.appWindowLayout?.[key]"
                           @input="(e: Event) => handleLayoutChange(key as keyof AppWindowLayout, (e.target as HTMLInputElement).value)"
                           @keydown="handleKeyDown"
                           class="w-24"
                           min="0"
                       />
                       <span class="text-sm text-muted-foreground">
                           {{ t('settings.appearance.layout.unit') }}
                       </span>
                   </div>
                </div>
            </template>
         </div>
      </div>
    </div>
  </div>
</template>
