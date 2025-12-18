<script setup lang="ts">
import { ref } from 'vue'
import { useSettingsStore } from '../store'
import { useFunctionActions } from '../composables/useFunctionActions'
import { storeToRefs } from 'pinia'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
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
import { useI18n } from '@/composables/useI18n'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { call } from '@/core/rpc'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const {
  updateWindowTitle,
  updateScreenshotDir,
  updateTaskbarLowerOnResize,
  updateLetterboxEnabled,
  updateRecordingOutputDir,
  updateRecordingFps,
  updateRecordingBitrate,
  updateRecordingEncoderMode,
  resetFunctionSettings,
} = useFunctionActions()
const { clearError } = store
const { t } = useI18n()

const isSelectingDir = ref(false)
const isSelectingRecordingDir = ref(false)
// Local state for input to avoid jitter
const inputTitle = ref(appSettings.value?.window?.targetTitle || '')
const inputBitrateMbps = ref(
  (appSettings.value?.features?.recording?.bitrate || 80000000) / 1000000
)

const handleTitleChange = async () => {
  const value = inputTitle.value.trim()
  if (value === '') {
    // TODO: toast error
    return
  }
  try {
    await updateWindowTitle(value)
    // TODO: toast success
  } catch (error) {
    console.error('Failed to update window title:', error)
    // TODO: toast error
  }
}

const handleKeyDown = (e: KeyboardEvent) => {
  if (e.key === 'Enter') {
    handleTitleChange()
  }
}

const handleSelectDir = async () => {
  isSelectingDir.value = true
  try {
    // TODO: check env for parentWindowMode
    const parentWindowMode = 2 // web: 2

    const result = await call<{ path: string }>(
      'dialog.openDirectory',
      {
        title: t('settings.function.screenshot.directory.dialogTitle'),
        parentWindowMode,
      },
      0
    )
    await updateScreenshotDir(result.path)
    // TODO: toast success
  } catch (error) {
    console.error('Failed to select screenshot directory:', error)
    // TODO: toast error
  } finally {
    isSelectingDir.value = false
  }
}

const handleSelectRecordingDir = async () => {
  isSelectingRecordingDir.value = true
  try {
    const parentWindowMode = 2 // web: 2

    const result = await call<{ path: string }>(
      'dialog.openDirectory',
      {
        title: t('settings.function.recording.outputDir.dialogTitle'),
        parentWindowMode,
      },
      0
    )
    await updateRecordingOutputDir(result.path)
    // TODO: toast success
  } catch (error) {
    console.error('Failed to select recording directory:', error)
    // TODO: toast error
  } finally {
    isSelectingRecordingDir.value = false
  }
}

const handleBitrateChange = async () => {
  const bitrateBps = inputBitrateMbps.value * 1000000
  await updateRecordingBitrate(bitrateBps)
  // TODO: toast success
}

const handleResetSettings = async () => {
  await resetFunctionSettings()
  inputBitrateMbps.value = 80 // Reset to default
  // TODO: toast success
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.function.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.function.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.function.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <!-- Header -->
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.function.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.function.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.function.reset.title')"
        :description="t('settings.function.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-8">
      <!-- Window Control -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.windowControl.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.windowControl.description') }}
          </p>
        </div>

        <ItemGroup>
          <Item variant="outline" size="sm">
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
                @keydown="handleKeyDown"
                :placeholder="t('settings.function.windowControl.windowTitle.placeholder')"
                class="w-48"
              />
              <Button @click="handleTitleChange" :disabled="!inputTitle.trim()" size="sm">
                {{ t('settings.function.windowControl.windowTitle.update') }}
              </Button>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.windowControl.taskbarLowerOnResize.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.windowControl.taskbarLowerOnResize.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Switch
                :model-value="appSettings?.window?.taskbar?.lowerOnResize"
                @update:model-value="updateTaskbarLowerOnResize"
              />
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>

      <!-- Screenshot -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.screenshot.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.screenshot.description') }}
          </p>
        </div>

        <Item variant="outline" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.screenshot.directory.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.screenshot.directory.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Input
              :model-value="appSettings?.features?.screenshot?.screenshotDirPath"
              readonly
              :placeholder="t('settings.function.screenshot.directory.placeholder')"
              class="w-48"
            />
            <Button @click="handleSelectDir" :disabled="isSelectingDir" size="sm">
              {{
                isSelectingDir
                  ? t('settings.function.screenshot.directory.selecting')
                  : t('settings.function.screenshot.directory.selectButton')
              }}
            </Button>
          </ItemActions>
        </Item>
      </div>

      <!-- Letterbox -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.letterbox.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.letterbox.description') }}
          </p>
        </div>

        <Item variant="outline" size="sm">
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

      <!-- Recording -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.recording.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.recording.description') }}
          </p>
        </div>

        <ItemGroup>
          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.outputDir.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.outputDir.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                :model-value="appSettings?.features?.recording?.outputDirPath"
                readonly
                :placeholder="t('settings.function.recording.outputDir.placeholder')"
                class="w-48"
              />
              <Button
                @click="handleSelectRecordingDir"
                :disabled="isSelectingRecordingDir"
                size="sm"
              >
                {{
                  isSelectingRecordingDir
                    ? t('settings.function.recording.outputDir.selecting')
                    : t('settings.function.recording.outputDir.selectButton')
                }}
              </Button>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.fps.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.fps.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="String(appSettings?.features?.recording?.fps)"
                @update:model-value="(value) => updateRecordingFps(Number(value))"
              >
                <SelectTrigger class="w-32">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="30">30 FPS</SelectItem>
                  <SelectItem value="60">60 FPS</SelectItem>
                  <SelectItem value="120">120 FPS</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.bitrate.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.bitrate.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputBitrateMbps"
                type="number"
                :min="1"
                :max="500"
                class="w-24"
              />
              <span class="text-sm text-muted-foreground">Mbps</span>
              <Button @click="handleBitrateChange" size="sm">
                {{ t('settings.function.windowControl.windowTitle.update') }}
              </Button>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.encoderMode.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.encoderMode.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="appSettings?.features?.recording?.encoderMode"
                @update:model-value="
                  (value) => updateRecordingEncoderMode(value as 'auto' | 'gpu' | 'cpu')
                "
              >
                <SelectTrigger class="w-48">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="auto">{{
                    t('settings.function.recording.encoderMode.auto')
                  }}</SelectItem>
                  <SelectItem value="gpu">{{
                    t('settings.function.recording.encoderMode.gpu')
                  }}</SelectItem>
                  <SelectItem value="cpu">{{
                    t('settings.function.recording.encoderMode.cpu')
                  }}</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>
    </div>
  </div>
</template>
