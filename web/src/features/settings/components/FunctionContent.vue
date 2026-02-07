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
  updateOutputDir,
  updateGameAlbumPath,
  updateTaskbarLowerOnResize,
  updateLetterboxEnabled,
  // Motion Photo
  updateMotionPhotoEnabled,
  updateMotionPhotoDuration,
  updateMotionPhotoResolution,
  updateMotionPhotoFps,
  updateMotionPhotoBitrate,
  updateMotionPhotoCodec,
  updateMotionPhotoAudioSource,
  updateMotionPhotoAudioBitrate,
  // Instant Replay
  updateReplayBufferDuration,
  // Recording
  updateRecordingFps,
  updateRecordingBitrate,
  updateRecordingQuality,
  updateRecordingQp,
  updateRecordingRateControl,
  updateRecordingEncoderMode,
  updateRecordingCodec,
  updateRecordingAudioSource,
  updateRecordingAudioBitrate,
  resetFunctionSettings,
} = useFunctionActions()
const { clearError } = store
const { t } = useI18n()

const isSelectingOutputDir = ref(false)
const isSelectingGameAlbumDir = ref(false)
// Local state for input to avoid jitter
const inputTitle = ref(appSettings.value?.window?.targetTitle || '')
const inputBitrateMbps = ref(
  (appSettings.value?.features?.recording?.bitrate || 80000000) / 1000000
)
const inputQuality = ref(appSettings.value?.features?.recording?.quality || 70)
const inputQp = ref(appSettings.value?.features?.recording?.qp || 23)
const inputAudioBitrateKbps = ref(
  (appSettings.value?.features?.recording?.audioBitrate || 320000) / 1000
)
const inputFps = ref(appSettings.value?.features?.recording?.fps || 60)

// Motion Photo local state
const inputMotionPhotoDuration = ref(appSettings.value?.features?.motionPhoto?.duration || 3)
const inputMotionPhotoFps = ref(appSettings.value?.features?.motionPhoto?.fps || 30)
const inputMotionPhotoBitrateMbps = ref(
  (appSettings.value?.features?.motionPhoto?.bitrate || 10000000) / 1000000
)
const inputMotionPhotoAudioBitrateKbps = ref(
  (appSettings.value?.features?.motionPhoto?.audioBitrate || 128000) / 1000
)

// Instant Replay local state
const inputReplayBufferDuration = ref(appSettings.value?.features?.replayBuffer?.duration || 30)

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

const handleSelectOutputDir = async () => {
  isSelectingOutputDir.value = true
  try {
    const parentWindowMode = 2 // web: 2

    const result = await call<{ path: string }>(
      'dialog.openDirectory',
      {
        title: t('settings.function.outputDir.dialogTitle'),
        parentWindowMode,
      },
      0
    )
    await updateOutputDir(result.path)
    // TODO: toast success
  } catch (error) {
    console.error('Failed to select output directory:', error)
    // TODO: toast error
  } finally {
    isSelectingOutputDir.value = false
  }
}

const handleSelectGameAlbumDir = async () => {
  isSelectingGameAlbumDir.value = true
  try {
    const parentWindowMode = 2 // web: 2

    const result = await call<{ path: string }>(
      'dialog.openDirectory',
      {
        title: t('settings.function.screenshot.gameAlbum.dialogTitle'),
        parentWindowMode,
      },
      0
    )
    await updateGameAlbumPath(result.path)
    // TODO: toast success
  } catch (error) {
    console.error('Failed to select game album directory:', error)
    // TODO: toast error
  } finally {
    isSelectingGameAlbumDir.value = false
  }
}

const handleBitrateChange = async () => {
  const bitrateBps = inputBitrateMbps.value * 1000000
  await updateRecordingBitrate(bitrateBps)
  // TODO: toast success
}

const handleQualityChange = async () => {
  await updateRecordingQuality(inputQuality.value)
  // TODO: toast success
}

const handleQpChange = async () => {
  await updateRecordingQp(inputQp.value)
  // TODO: toast success
}

const handleAudioBitrateChange = async () => {
  const audioBitrateBps = inputAudioBitrateKbps.value * 1000
  await updateRecordingAudioBitrate(audioBitrateBps)
  // TODO: toast success
}

const handleFpsChange = async () => {
  if (!inputFps.value) return
  await updateRecordingFps(inputFps.value)
}

// Motion Photo handlers
const handleMotionPhotoDurationChange = async () => {
  if (!inputMotionPhotoDuration.value) return
  await updateMotionPhotoDuration(inputMotionPhotoDuration.value)
}

const handleMotionPhotoFpsChange = async () => {
  if (!inputMotionPhotoFps.value) return
  await updateMotionPhotoFps(inputMotionPhotoFps.value)
}

const handleMotionPhotoBitrateChange = async () => {
  const bitrateBps = inputMotionPhotoBitrateMbps.value * 1000000
  await updateMotionPhotoBitrate(bitrateBps)
}

const handleMotionPhotoAudioBitrateChange = async () => {
  const audioBitrateBps = inputMotionPhotoAudioBitrateKbps.value * 1000
  await updateMotionPhotoAudioBitrate(audioBitrateBps)
}

// Instant Replay handlers
const handleReplayBufferDurationChange = async () => {
  if (!inputReplayBufferDuration.value) return
  await updateReplayBufferDuration(inputReplayBufferDuration.value)
}

const handleResetSettings = async () => {
  await resetFunctionSettings()
  // Recording defaults
  inputBitrateMbps.value = 80
  inputQuality.value = 100
  inputQp.value = 23
  inputAudioBitrateKbps.value = 320
  inputFps.value = 60
  // Motion Photo defaults
  inputMotionPhotoDuration.value = 3
  inputMotionPhotoFps.value = 30
  inputMotionPhotoBitrateMbps.value = 10
  inputMotionPhotoAudioBitrateKbps.value = 128
  // Instant Replay defaults
  inputReplayBufferDuration.value = 30
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
      <!-- Output Directory -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.outputDir.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.outputDir.description') }}
          </p>
        </div>

        <Item variant="outline" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.outputDir.label') }}
            </ItemTitle>
            <ItemDescription>
              <template v-if="appSettings?.features?.outputDirPath">
                {{ appSettings.features.outputDirPath }}
              </template>
              <template v-else>
                {{ t('settings.function.outputDir.default') }}
              </template>
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Button @click="handleSelectOutputDir" :disabled="isSelectingOutputDir" size="sm">
              {{
                isSelectingOutputDir
                  ? t('settings.function.outputDir.selecting')
                  : t('settings.function.outputDir.selectButton')
              }}
            </Button>
          </ItemActions>
        </Item>
      </div>

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
                @keydown.enter="handleTitleChange"
                @blur="handleTitleChange"
                :placeholder="t('settings.function.windowControl.windowTitle.placeholder')"
                class="w-48"
              />
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
              {{ t('settings.function.screenshot.gameAlbum.label') }}
            </ItemTitle>
            <ItemDescription>
              <template v-if="appSettings?.features?.screenshot?.gameAlbumPath">
                {{ appSettings.features.screenshot.gameAlbumPath }}
              </template>
              <template v-else>
                {{ t('settings.function.screenshot.gameAlbum.description') }}
              </template>
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Button @click="handleSelectGameAlbumDir" :disabled="isSelectingGameAlbumDir" size="sm">
              {{
                isSelectingGameAlbumDir
                  ? t('settings.function.screenshot.gameAlbum.selecting')
                  : t('settings.function.screenshot.gameAlbum.selectButton')
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

      <!-- Motion Photo -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.motionPhoto.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.motionPhoto.description') }}
          </p>
        </div>

        <ItemGroup>
          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.enabled.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.enabled.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Switch
                :model-value="appSettings?.features?.motionPhoto?.enabled"
                @update:model-value="updateMotionPhotoEnabled"
              />
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.duration.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.duration.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputMotionPhotoDuration"
                type="number"
                :min="1"
                :max="10"
                class="w-24"
                @blur="handleMotionPhotoDurationChange"
                @keydown.enter="handleMotionPhotoDurationChange"
              />
              <span class="text-sm text-muted-foreground">s</span>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.resolution.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.resolution.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="String(appSettings?.features?.motionPhoto?.resolution || 1080)"
                @update:model-value="(value) => updateMotionPhotoResolution(Number(value))"
              >
                <SelectTrigger class="w-32">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="720">720P</SelectItem>
                  <SelectItem value="1080">1080P</SelectItem>
                  <SelectItem value="1440">2K</SelectItem>
                  <SelectItem value="2160">4K</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.fps.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.fps.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputMotionPhotoFps"
                type="number"
                :min="15"
                :max="60"
                class="w-24"
                @blur="handleMotionPhotoFpsChange"
                @keydown.enter="handleMotionPhotoFpsChange"
              />
              <span class="text-sm text-muted-foreground">FPS</span>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.bitrate.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.bitrate.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputMotionPhotoBitrateMbps"
                type="number"
                :min="1"
                :max="50"
                class="w-24"
                @blur="handleMotionPhotoBitrateChange"
                @keydown.enter="handleMotionPhotoBitrateChange"
              />
              <span class="text-sm text-muted-foreground">Mbps</span>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.codec.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.codec.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="appSettings?.features?.motionPhoto?.codec"
                @update:model-value="(value) => updateMotionPhotoCodec(value as 'h264' | 'h265')"
              >
                <SelectTrigger class="w-40">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="h264">H.264 / AVC</SelectItem>
                  <SelectItem value="h265">H.265 / HEVC</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.audioSource.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.audioSource.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="appSettings?.features?.motionPhoto?.audioSource"
                @update:model-value="
                  (value) => updateMotionPhotoAudioSource(value as 'none' | 'system' | 'game_only')
                "
              >
                <SelectTrigger class="w-40">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="none">{{
                    t('settings.function.recording.audioSource.none')
                  }}</SelectItem>
                  <SelectItem value="system">{{
                    t('settings.function.recording.audioSource.system')
                  }}</SelectItem>
                  <SelectItem value="game_only">{{
                    t('settings.function.recording.audioSource.gameOnly')
                  }}</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.motionPhoto.audioBitrate.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.motionPhoto.audioBitrate.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputMotionPhotoAudioBitrateKbps"
                type="number"
                :min="64"
                :max="320"
                class="w-24"
                @blur="handleMotionPhotoAudioBitrateChange"
                @keydown.enter="handleMotionPhotoAudioBitrateChange"
              />
              <span class="text-sm text-muted-foreground">kbps</span>
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>

      <!-- Instant Replay -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.function.replayBuffer.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.function.replayBuffer.description') }}
          </p>
        </div>

        <Item variant="outline" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.function.replayBuffer.duration.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.function.replayBuffer.duration.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Input
              v-model.number="inputReplayBufferDuration"
              type="number"
              :min="5"
              :max="300"
              class="w-24"
              @blur="handleReplayBufferDurationChange"
              @keydown.enter="handleReplayBufferDurationChange"
            />
            <span class="text-sm text-muted-foreground">s</span>
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
                {{ t('settings.function.recording.fps.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.fps.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputFps"
                type="number"
                :min="1"
                class="w-24"
                @blur="handleFpsChange"
                @keydown.enter="handleFpsChange"
              />
              <span class="text-sm text-muted-foreground">FPS</span>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.rateControl.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.rateControl.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="appSettings?.features?.recording?.rateControl"
                @update:model-value="
                  (value) => updateRecordingRateControl(value as 'cbr' | 'vbr' | 'manual_qp')
                "
              >
                <SelectTrigger class="w-48">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="cbr">{{
                    t('settings.function.recording.rateControl.cbr')
                  }}</SelectItem>
                  <SelectItem value="vbr">{{
                    t('settings.function.recording.rateControl.vbr')
                  }}</SelectItem>
                  <SelectItem value="manual_qp">{{
                    t('settings.function.recording.rateControl.manualQp')
                  }}</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>

          <Item
            v-if="appSettings?.features?.recording?.rateControl === 'cbr'"
            variant="outline"
            size="sm"
          >
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
                @blur="handleBitrateChange"
                @keydown.enter="handleBitrateChange"
              />
              <span class="text-sm text-muted-foreground">Mbps</span>
            </ItemActions>
          </Item>

          <Item
            v-if="appSettings?.features?.recording?.rateControl === 'vbr'"
            variant="outline"
            size="sm"
          >
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.quality.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.quality.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputQuality"
                type="number"
                :min="0"
                :max="100"
                class="w-24"
                @blur="handleQualityChange"
                @keydown.enter="handleQualityChange"
              />
              <span class="text-sm text-muted-foreground">(0-100)</span>
            </ItemActions>
          </Item>

          <Item
            v-if="appSettings?.features?.recording?.rateControl === 'manual_qp'"
            variant="outline"
            size="sm"
          >
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.qp.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.qp.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputQp"
                type="number"
                :min="0"
                :max="51"
                class="w-24"
                @blur="handleQpChange"
                @keydown.enter="handleQpChange"
              />
              <span class="text-sm text-muted-foreground">(0-51)</span>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.codec.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.codec.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="appSettings?.features?.recording?.codec"
                @update:model-value="(value) => updateRecordingCodec(value as 'h264' | 'h265')"
              >
                <SelectTrigger class="w-48">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="h264">H.264 / AVC</SelectItem>
                  <SelectItem value="h265">H.265 / HEVC</SelectItem>
                </SelectContent>
              </Select>
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

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.audioSource.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.audioSource.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Select
                :model-value="appSettings?.features?.recording?.audioSource"
                @update:model-value="
                  (value) => updateRecordingAudioSource(value as 'none' | 'system' | 'game_only')
                "
              >
                <SelectTrigger class="w-48">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="none">{{
                    t('settings.function.recording.audioSource.none')
                  }}</SelectItem>
                  <SelectItem value="system">{{
                    t('settings.function.recording.audioSource.system')
                  }}</SelectItem>
                  <SelectItem value="game_only">{{
                    t('settings.function.recording.audioSource.gameOnly')
                  }}</SelectItem>
                </SelectContent>
              </Select>
            </ItemActions>
          </Item>

          <Item variant="outline" size="sm">
            <ItemContent>
              <ItemTitle>
                {{ t('settings.function.recording.audioBitrate.label') }}
              </ItemTitle>
              <ItemDescription>
                {{ t('settings.function.recording.audioBitrate.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Input
                v-model.number="inputAudioBitrateKbps"
                type="number"
                :min="64"
                :max="512"
                class="w-24"
                @blur="handleAudioBitrateChange"
                @keydown.enter="handleAudioBitrateChange"
              />
              <span class="text-sm text-muted-foreground">kbps</span>
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>
    </div>
  </div>
</template>
