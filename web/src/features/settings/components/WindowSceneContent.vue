<script setup lang="ts">
import { ref, watch } from 'vue'
import { useSettingsStore } from '../store'
import { useFunctionActions } from '../composables/useFunctionActions'
import { storeToRefs } from 'pinia'
import WindowTitlePickerButton from '@/components/WindowTitlePickerButton.vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Item, ItemContent, ItemTitle, ItemDescription, ItemActions } from '@/components/ui/item'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { useI18n } from '@/composables/useI18n'
import ResetSettingsDialog from './ResetSettingsDialog.vue'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { updateWindowTitle, updateWindowResetResolution, resetWindowSceneSettings } =
  useFunctionActions()
const { clearError } = store
const { t } = useI18n()

type ResetResolutionMode = 'screen' | 'custom'

const inputTitle = ref(appSettings.value?.window?.targetTitle || '')
const isEditingTitle = ref(false)
const resetResolutionMode = ref<ResetResolutionMode>('screen')
const inputResetWidth = ref(appSettings.value?.window?.resetResolution?.width || 0)
const inputResetHeight = ref(appSettings.value?.window?.resetResolution?.height || 0)

watch(
  () => appSettings.value?.window?.targetTitle,
  (newTitle) => {
    if (!isEditingTitle.value) {
      inputTitle.value = newTitle || ''
    }
  },
  { immediate: true }
)

watch(
  () => appSettings.value?.window?.resetResolution,
  (newResetResolution) => {
    const width = newResetResolution?.width ?? 0
    const height = newResetResolution?.height ?? 0
    inputResetWidth.value = width
    inputResetHeight.value = height
    resetResolutionMode.value = width > 0 && height > 0 ? 'custom' : 'screen'
  },
  { immediate: true, deep: true }
)

const handleTitleChange = async () => {
  const nextTitle = inputTitle.value.trim() === '' ? '' : inputTitle.value

  try {
    await updateWindowTitle(nextTitle)
  } catch (error) {
    console.error('Failed to update window title:', error)
  }
}

const handleTitlePicked = async (title: string) => {
  inputTitle.value = title
  isEditingTitle.value = false

  try {
    await updateWindowTitle(title)
  } catch (error) {
    console.error('Failed to update window title:', error)
  }
}

const handleResetResolutionModeChange = async (value: string) => {
  const mode = value as ResetResolutionMode
  resetResolutionMode.value = mode

  try {
    if (mode === 'screen') {
      inputResetWidth.value = 0
      inputResetHeight.value = 0
      await updateWindowResetResolution(0, 0)
      return
    }

    const width = inputResetWidth.value > 0 ? inputResetWidth.value : 1920
    const height = inputResetHeight.value > 0 ? inputResetHeight.value : 1080
    inputResetWidth.value = width
    inputResetHeight.value = height
    await updateWindowResetResolution(width, height)
  } catch (error) {
    console.error('Failed to update reset resolution mode:', error)
  }
}

const handleResetResolutionChange = async () => {
  const width = Number(inputResetWidth.value)
  const height = Number(inputResetHeight.value)
  if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
    return
  }

  try {
    await updateWindowResetResolution(Math.trunc(width), Math.trunc(height))
  } catch (error) {
    console.error('Failed to update reset resolution:', error)
  }
}

const handleResetSettings = async () => {
  await resetWindowSceneSettings()
  inputTitle.value = appSettings.value?.window?.targetTitle || ''
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.windowScene.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.windowScene.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.windowScene.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.windowScene.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.windowScene.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.windowScene.reset.title')"
        :description="t('settings.windowScene.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-8">
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.windowControl.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.windowControl.description') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.windowControl.windowTitle.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.windowControl.windowTitle.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <div class="flex items-center gap-2">
              <Input
                v-model="inputTitle"
                @focus="isEditingTitle = true"
                @keydown.enter="handleTitleChange"
                @blur="
                  () => {
                    isEditingTitle = false
                    handleTitleChange()
                  }
                "
                :placeholder="t('settings.function.windowControl.windowTitle.placeholder')"
                class="w-48"
              />
              <WindowTitlePickerButton @select="handleTitlePicked" />
            </div>
          </ItemActions>
        </Item>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.windowControl.resetResolution.mode.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.windowControl.resetResolution.mode.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="resetResolutionMode"
              @update:model-value="(value) => handleResetResolutionModeChange(String(value))"
            >
              <SelectTrigger class="w-36">
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="screen">
                  {{ t('settings.function.windowControl.resetResolution.mode.screen') }}
                </SelectItem>
                <SelectItem value="custom">
                  {{ t('settings.function.windowControl.resetResolution.mode.custom') }}
                </SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>

        <Item v-if="resetResolutionMode === 'custom'" variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.windowControl.resetResolution.width.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.windowControl.resetResolution.width.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Input
              v-model.number="inputResetWidth"
              type="number"
              :min="1"
              class="w-24"
              @blur="handleResetResolutionChange"
              @keydown.enter="handleResetResolutionChange"
            />
            <span class="text-sm text-muted-foreground">px</span>
          </ItemActions>
        </Item>

        <Item v-if="resetResolutionMode === 'custom'" variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.windowControl.resetResolution.height.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.windowControl.resetResolution.height.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Input
              v-model.number="inputResetHeight"
              type="number"
              :min="1"
              class="w-24"
              @blur="handleResetResolutionChange"
              @keydown.enter="handleResetResolutionChange"
            />
            <span class="text-sm text-muted-foreground">px</span>
          </ItemActions>
        </Item>
      </div>
    </div>
  </div>
</template>
