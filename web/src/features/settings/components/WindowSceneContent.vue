<script setup lang="ts">
import { ref, watch } from 'vue'
import { useSettingsStore } from '../store'
import { useFunctionActions } from '../composables/useFunctionActions'
import { storeToRefs } from 'pinia'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Switch } from '@/components/ui/switch'
import { Item, ItemContent, ItemTitle, ItemDescription, ItemActions } from '@/components/ui/item'
import { useI18n } from '@/composables/useI18n'
import ResetSettingsDialog from './ResetSettingsDialog.vue'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { updateWindowTitle, updateLetterboxEnabled, resetWindowSceneSettings } = useFunctionActions()
const { clearError } = store
const { t } = useI18n()

const inputTitle = ref(appSettings.value?.window?.targetTitle || '')
const isEditingTitle = ref(false)

watch(
  () => appSettings.value?.window?.targetTitle,
  (newTitle) => {
    if (!isEditingTitle.value) {
      inputTitle.value = newTitle || ''
    }
  },
  { immediate: true }
)

const handleTitleChange = async () => {
  const value = inputTitle.value.trim()
  if (value === '') {
    return
  }
  try {
    await updateWindowTitle(value)
  } catch (error) {
    console.error('Failed to update window title:', error)
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
          </ItemActions>
        </Item>
      </div>

      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.letterbox.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.letterbox.description') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.letterbox.enabled.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.letterbox.enabled.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Switch
              :model-value="appSettings?.features?.letterbox?.enabled"
              @update:model-value="updateLetterboxEnabled"
            />
          </ItemActions>
        </Item>
      </div>
    </div>
  </div>
</template>
