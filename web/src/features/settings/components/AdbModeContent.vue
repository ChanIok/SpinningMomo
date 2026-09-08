<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { on, off } from '@/core/rpc'
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
import { adbModeApi, type AdbModeStatus } from '../api'
import { useSettingsStore } from '../store'
import { DEFAULT_APP_SETTINGS, type AppSettings } from '../types'
import { storeToRefs } from 'pinia'
import { RotateCcw } from '@lucide/vue'
import ResetSettingsDialog from './ResetSettingsDialog.vue'

const store = useSettingsStore()
const { appSettings } = storeToRefs(store)
const { t } = useI18n()
const { toast } = useToast()

const status = ref<AdbModeStatus | null>(null)
const statusError = ref<string | null>(null)
const isLoadingStatus = ref(false)
const isSelectingAdb = ref(false)
const isEditing = ref(false)

const adbModeSettings = computed(() => appSettings.value.features.adbMode)

const statusLabel = computed(() => {
  if (!status.value) return t('settings.adbMode.status.unknown')
  switch (status.value.connectionState) {
    case 'connected':
      return t('settings.adbMode.status.connected')
    case 'connecting':
      return t('settings.adbMode.status.connecting')
    case 'restoring':
      return t('settings.adbMode.status.restoring')
    case 'error':
      return t('settings.adbMode.status.error')
    default:
      return t('settings.adbMode.status.disconnected')
  }
})

const displayLabel = computed(() => {
  if (!status.value || status.value.displayWidth <= 0 || status.value.displayHeight <= 0) {
    return t('settings.adbMode.display.empty')
  }
  return `${status.value.displayWidth} × ${status.value.displayHeight}`
})

const getErrorMessage = (error: unknown): string => {
  return error instanceof Error ? error.message : String(error)
}

const refreshStatus = async () => {
  if (isLoadingStatus.value) return
  isLoadingStatus.value = true
  try {
    status.value = await adbModeApi.getStatus()
    statusError.value = null
  } catch (error) {
    statusError.value = getErrorMessage(error)
  } finally {
    isLoadingStatus.value = false
  }
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
    await refreshStatus()
  } catch (error) {
    toast.error(t('settings.adbMode.saveFailed'), { description: getErrorMessage(error) })
  }
}

const handleStatusChanged = (params: unknown) => {
  if (params && typeof params === 'object') {
    status.value = params as AdbModeStatus
  }
}

watch(
  () => appSettings.value.features.adbMode,
  () => {
    if (!isEditing.value) void refreshStatus()
  },
  { deep: true }
)

onMounted(() => {
  if (!isLocalAccess()) return
  void refreshStatus()
  on('adbMode.changed', handleStatusChanged)
})

onBeforeUnmount(() => {
  if (!isLocalAccess()) return
  off('adbMode.changed', handleStatusChanged)
})
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
      <!-- 连接状态是运行时反馈，始终置于设置项最上方。 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.status.label') }}</ItemTitle>
          <ItemDescription>
            {{ statusLabel }}<span v-if="status?.serial"> · {{ status.serial }}</span>
            <span v-if="status?.restorePending">
              · {{ t('settings.adbMode.status.pendingRestore') }}</span
            >
            <span v-if="status?.connected"> · {{ displayLabel }}</span>
          </ItemDescription>
          <p v-if="statusError || status?.lastError" class="mt-1 text-xs text-destructive">
            {{ statusError || status?.lastError }}
          </p>
        </ItemContent>
      </Item>

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

      <Item v-if="adbModeSettings.useCustomAdbPath" variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.endpoint.label') }}</ItemTitle>
          <ItemDescription>{{ t('settings.adbMode.endpoint.description') }}</ItemDescription>
        </ItemContent>
        <ItemActions>
          <div class="flex items-center gap-2">
            <Input
              :model-value="adbModeSettings.host"
              class="w-36 font-mono text-xs"
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
              class="w-24"
              @blur="
                (event: FocusEvent) => handlePortChange((event.target as HTMLInputElement).value)
              "
              @keydown.enter="(event: KeyboardEvent) => (event.target as HTMLInputElement).blur()"
            />
          </div>
        </ItemActions>
      </Item>

      <Item v-if="adbModeSettings.useCustomAdbPath" variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.adbMode.serial.label') }}</ItemTitle>
          <ItemDescription>{{ t('settings.adbMode.serial.description') }}</ItemDescription>
        </ItemContent>
        <ItemActions>
          <Input
            :model-value="adbModeSettings.serial"
            class="w-48 font-mono text-xs"
            :placeholder="t('settings.adbMode.serial.placeholder')"
            @focus="isEditing = true"
            @blur="
              (event: FocusEvent) =>
                handleTextChange('serial', (event.target as HTMLInputElement).value)
            "
            @keydown.enter="(event: KeyboardEvent) => (event.target as HTMLInputElement).blur()"
          />
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
