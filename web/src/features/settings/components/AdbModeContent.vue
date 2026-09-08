<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { isLocalAccess } from '@/core/access'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Switch } from '@/components/ui/switch'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import {
  Item,
  ItemActions,
  ItemContent,
  ItemDescription,
  ItemGroup,
  ItemTitle,
} from '@/components/ui/item'
import { adbModeApi } from '../api'
import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS, type AppSettings } from '../types'
import { storeToRefs } from 'pinia'
import { RotateCcw, Loader2 } from '@lucide/vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import AdbDeviceInput from '@/components/AdbDeviceInput.vue'

const store = useSettingsStore()
const { appSettings } = storeToRefs(store)
const { t } = useI18n()
const { toast } = useToast()

const isSelectingAdb = ref(false)
const isEditing = ref(false)
const isConnectingEndpoint = ref(false)

const adbModeSettings = computed(() => appSettings.value.features.adbMode)
const inputSerial = ref(adbModeSettings.value?.serial || '')

watch(
  () => adbModeSettings.value?.serial,
  (newSerial) => {
    inputSerial.value = newSerial || ''
  },
  { immediate: true }
)

const getErrorMessage = (error: unknown): string => {
  return error instanceof Error ? error.message : String(error)
}

const updateSettings = async (patch: Partial<AppSettings['features']['adbMode']>) => {
  await store.updateSettings({
    features: {
      ...appSettings.value.features,
      adbMode: {
        ...adbModeSettings.value,
        ...patch,
      },
    },
  })
}

const handleCustomAdbPathChange = async (useCustomAdbPath: boolean) => {
  try {
    await updateSettings({ useCustomAdbPath })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleDevicePicked = async (serial: string) => {
  inputSerial.value = serial
  try {
    await updateSettings({ serial })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleConnectEndpoint = async () => {
  if (isConnectingEndpoint.value) return
  const host = adbModeSettings.value?.host?.trim() || '127.0.0.1'
  const port = adbModeSettings.value?.port || 7555

  isConnectingEndpoint.value = true
  try {
    const result = await adbModeApi.connectEndpoint({ host, port })
    if (result.success) {
      toast.success(t('settings.adbMode.networkDevice.connectSuccess', { endpoint: result.serial }))
      await handleDevicePicked(result.serial)
    } else {
      toast.error(t('settings.adbMode.networkDevice.connectFailed'), {
        description: result.error || undefined,
      })
    }
  } catch (error) {
    toast.error(t('settings.adbMode.networkDevice.connectFailed'), {
      description: getErrorMessage(error),
    })
  } finally {
    isConnectingEndpoint.value = false
  }
}

const handleTextChange = async (key: 'adbPath' | 'host' | 'serial', value: string) => {
  try {
    await updateSettings({ [key]: value } as Partial<AppSettings['features']['adbMode']>)
    isEditing.value = false
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleSelectAdb = async () => {
  if (isSelectingAdb.value) return
  isSelectingAdb.value = true
  try {
    const path = await adbModeApi.selectAdbExecutable(t('settings.adbMode.adbPath.dialogTitle'))
    if (path) {
      await updateSettings({ adbPath: path })
    }
  } catch (error) {
    const message = getErrorMessage(error)
    if (!message.toLowerCase().includes('cancel')) {
      toast.error(t('settings.adbMode.saveFailed'), { description: message })
    }
  } finally {
    isSelectingAdb.value = false
  }
}

const handlePortChange = async (value: string) => {
  const port = Number.parseInt(value, 10)
  if (!Number.isFinite(port) || port < 1 || port > 65535) return
  try {
    await updateSettings({ port })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const inputRecordBitrateMbps = ref(
  Math.round((adbModeSettings.value?.recordBitrate || 40000000) / 1000000)
)

watch(
  () => adbModeSettings.value?.recordBitrate,
  (val) => {
    if (val) {
      inputRecordBitrateMbps.value = Math.round(val / 1000000)
    }
  }
)

const handleRecordBitrateChange = async () => {
  const mbps = Number(inputRecordBitrateMbps.value)
  if (isNaN(mbps) || mbps <= 0) {
    inputRecordBitrateMbps.value = Math.round(
      (adbModeSettings.value?.recordBitrate || 40000000) / 1000000
    )
    return
  }
  const bitrateBps = Math.round(mbps * 1000000)
  try {
    await updateSettings({ recordBitrate: bitrateBps })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleRecordFpsChange = async (fps: number) => {
  try {
    await updateSettings({ recordFps: fps })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleRecordCodecChange = async (codec: 'h264' | 'h265') => {
  try {
    await updateSettings({ recordCodec: codec })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleReset = async () => {
  try {
    await updateSettings({ ...DEFAULT_APP_SETTINGS.features.adbMode })
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}
</script>

<template>
  <div v-if="isLocalAccess()" class="space-y-4">
    <div>
      <h3 class="text-lg font-semibold text-foreground">
        {{ t('settings.adbMode.title') }}
      </h3>
      <p class="mt-1 text-sm text-muted-foreground">
        {{ t('settings.adbMode.description') }}
      </p>
    </div>

    <ItemGroup>
      <!-- 目标设备（通用） -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.targetDevice.label') }}</ItemTitle>
          <ItemDescription>{{ t('settings.adbMode.targetDevice.description') }}</ItemDescription>
        </ItemContent>
        <ItemActions>
          <AdbDeviceInput v-model="inputSerial" @select="handleDevicePicked" class="w-64" />
        </ItemActions>
      </Item>

      <!-- 自定义 ADB 路径 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.customPath.label') }}</ItemTitle>
          <ItemDescription>{{ t('settings.adbMode.customPath.description') }}</ItemDescription>
        </ItemContent>
        <ItemActions>
          <Switch
            :model-value="adbModeSettings.useCustomAdbPath"
            @update:model-value="(value) => handleCustomAdbPathChange(Boolean(value))"
          />
        </ItemActions>
      </Item>

      <Item v-if="adbModeSettings.useCustomAdbPath" variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.adbPath.label') }}</ItemTitle>
          <ItemDescription>
            <template v-if="adbModeSettings.adbPath">
              {{ adbModeSettings.adbPath }}
            </template>
            <template v-else>
              {{ t('settings.adbMode.adbPath.placeholder') }}
            </template>
          </ItemDescription>
        </ItemContent>
        <ItemActions>
          <Button size="sm" :disabled="isSelectingAdb" @click="handleSelectAdb">
            {{
              isSelectingAdb
                ? t('settings.adbMode.adbPath.selecting')
                : t('settings.adbMode.adbPath.select')
            }}
          </Button>
        </ItemActions>
      </Item>

      <!-- 网络设备调试（无线调试 / 自定义模拟器，仅当开启自定义 ADB 路径时显示） -->
      <Item v-if="adbModeSettings.useCustomAdbPath" variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.networkDevice.label') }}</ItemTitle>
          <ItemDescription>{{ t('settings.adbMode.networkDevice.description') }}</ItemDescription>
        </ItemContent>
        <ItemActions>
          <div class="flex items-center gap-2">
            <Input
              :model-value="adbModeSettings.host"
              placeholder="127.0.0.1"
              class="w-32 font-mono text-xs"
              @focus="isEditing = true"
              @blur="
                (event: FocusEvent) =>
                  handleTextChange('host', (event.target as HTMLInputElement).value)
              "
              @keydown.enter="(event: KeyboardEvent) => (event.target as HTMLInputElement).blur()"
            />
            <span class="text-sm text-muted-foreground">:</span>
            <Input
              :model-value="adbModeSettings.port"
              type="number"
              min="1"
              max="65535"
              placeholder="7555"
              class="w-20 font-mono text-xs"
              @blur="
                (event: FocusEvent) => handlePortChange((event.target as HTMLInputElement).value)
              "
              @keydown.enter="(event: KeyboardEvent) => (event.target as HTMLInputElement).blur()"
            />
            <Button
              size="sm"
              variant="outline"
              :disabled="isConnectingEndpoint"
              @click="handleConnectEndpoint"
            >
              <Loader2 v-if="isConnectingEndpoint" class="h-3.5 w-3.5 animate-spin" />
              <template v-else>{{ t('settings.adbMode.networkDevice.connect') }}</template>
            </Button>
          </div>
        </ItemActions>
      </Item>

      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.autoConnect.label') }}</ItemTitle>
          <ItemDescription>{{ t('settings.adbMode.autoConnect.description') }}</ItemDescription>
        </ItemContent>
        <ItemActions>
          <Switch
            :model-value="adbModeSettings.autoConnect"
            @update:model-value="(value) => updateSettings({ autoConnect: Boolean(value) })"
          />
        </ItemActions>
      </Item>

      <!-- 分辨率按长边计算：规避模拟器/真机长边上限 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.useResolutionLongEdge.label') }}</ItemTitle>
          <ItemDescription>
            {{ t('settings.adbMode.useResolutionLongEdge.description') }}
          </ItemDescription>
        </ItemContent>
        <ItemActions>
          <Switch
            :model-value="adbModeSettings.useResolutionLongEdge"
            @update:model-value="
              (value) => updateSettings({ useResolutionLongEdge: Boolean(value) })
            "
          />
        </ItemActions>
      </Item>
    </ItemGroup>

    <div class="pt-4">
      <h4 class="text-sm font-medium text-foreground">
        {{ t('settings.adbMode.recording.groupTitle') }}
      </h4>
    </div>

    <ItemGroup>
      <!-- 录制码率 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.recording.bitrate.label') }}</ItemTitle>
          <ItemDescription>
            {{ t('settings.adbMode.recording.bitrate.description') }}
          </ItemDescription>
        </ItemContent>
        <ItemActions>
          <Input
            v-model.number="inputRecordBitrateMbps"
            type="number"
            :min="1"
            :max="200"
            class="w-24"
            @blur="handleRecordBitrateChange"
            @keydown.enter="handleRecordBitrateChange"
          />
          <span class="text-sm text-muted-foreground">Mbps</span>
        </ItemActions>
      </Item>

      <!-- 录制帧率 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.recording.fps.label') }}</ItemTitle>
          <ItemDescription>
            {{ t('settings.adbMode.recording.fps.description') }}
          </ItemDescription>
        </ItemContent>
        <ItemActions>
          <Select
            :model-value="String(adbModeSettings.recordFps || 60)"
            @update:model-value="(value) => handleRecordFpsChange(Number(value))"
          >
            <SelectTrigger class="w-32">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="30">30 FPS</SelectItem>
              <SelectItem value="60">60 FPS</SelectItem>
            </SelectContent>
          </Select>
        </ItemActions>
      </Item>

      <!-- 编码格式 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.recording.codec.label') }}</ItemTitle>
          <ItemDescription>
            {{ t('settings.adbMode.recording.codec.description') }}
          </ItemDescription>
        </ItemContent>
        <ItemActions>
          <Select
            :model-value="adbModeSettings.recordCodec || 'h264'"
            @update:model-value="(value) => handleRecordCodecChange(value as 'h264' | 'h265')"
          >
            <SelectTrigger class="w-40">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="h264">
                {{ t('settings.adbMode.recording.codec.h264') }}
              </SelectItem>
              <SelectItem value="h265">
                {{ t('settings.adbMode.recording.codec.h265') }}
              </SelectItem>
            </SelectContent>
          </Select>
        </ItemActions>
      </Item>
    </ItemGroup>

    <div class="mt-12 flex justify-center pb-4">
      <ResetSettingsDialog
        :title="t('settings.adbMode.reset.title')"
        :description="t('settings.adbMode.reset.description')"
        @reset="handleReset"
      >
        <template #trigger>
          <Button
            variant="ghost"
            size="sm"
            class="gap-1.5 text-xs text-muted-foreground/60 transition-colors hover:bg-destructive/10 hover:text-destructive"
          >
            <RotateCcw class="h-3.5 w-3.5" />
            {{ t('settings.adbMode.reset.title') }}
          </Button>
        </template>
      </ResetSettingsDialog>
    </div>
  </div>
</template>
