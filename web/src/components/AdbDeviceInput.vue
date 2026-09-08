<script setup lang="ts">
import type { HTMLAttributes } from 'vue'
import { computed, onMounted, ref } from 'vue'
import { useVModel } from '@vueuse/core'
import { Smartphone, MonitorSmartphone } from '@lucide/vue'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { cn } from '@/lib/utils'
import { useI18n } from '@/composables/useI18n'
import { adbModeApi, type DiscoveredAdbDevice } from '@/features/settings/api'

defineOptions({
  inheritAttrs: false,
})

const props = defineProps<{
  defaultValue?: string
  modelValue?: string
  disabled?: boolean
  class?: HTMLAttributes['class']
}>()

const emit = defineEmits<{
  (e: 'update:modelValue', payload: string): void
  (e: 'select', serial: string): void
}>()

const { t } = useI18n()

const modelValue = useVModel(props, 'modelValue', emit, {
  passive: true,
  defaultValue: props.defaultValue,
})

const AUTO_VALUE = '__auto__'

const isLoading = ref(false)
const loadFailed = ref(false)
const devices = ref<DiscoveredAdbDevice[]>([])

const selectedDevice = computed(() => {
  if (!modelValue.value) return null
  return devices.value.find((d) => d.serial === modelValue.value) || null
})

const getDeviceDisplayName = (device: DiscoveredAdbDevice): string => {
  if (device.kind === 'mumu') return t('settings.adbMode.targetDevice.deviceKind.mumu')
  if (device.kind === 'ldplayer') return t('settings.adbMode.targetDevice.deviceKind.ldplayer')
  if (device.kind === 'bluestacks') return t('settings.adbMode.targetDevice.deviceKind.bluestacks')
  if (device.kind === 'emulator') return t('settings.adbMode.targetDevice.deviceKind.emulator')
  if (device.model) return device.model
  return t('settings.adbMode.targetDevice.deviceKind.device')
}

const displayLabel = computed(() => {
  if (!modelValue.value) {
    return t('settings.adbMode.targetDevice.auto')
  }
  if (selectedDevice.value) {
    return `${getDeviceDisplayName(selectedDevice.value)} (${selectedDevice.value.serial})`
  }
  return modelValue.value
})

const internalValue = computed(() => {
  return modelValue.value || AUTO_VALUE
})

const loadDevices = async () => {
  isLoading.value = true
  loadFailed.value = false

  try {
    devices.value = await adbModeApi.listDevices()
  } catch (error) {
    devices.value = []
    loadFailed.value = true
    console.error('Failed to list ADB devices:', error)
  } finally {
    isLoading.value = false
  }
}

const handleOpenChange = (nextOpen: boolean) => {
  if (nextOpen) {
    void loadDevices()
  }
}

const handleSelect = (value: string) => {
  const nextSerial = value === AUTO_VALUE ? '' : value
  modelValue.value = nextSerial
  emit('select', nextSerial)
}

onMounted(() => {
  if (modelValue.value) {
    void loadDevices()
  }
})
</script>

<template>
  <Select
    :model-value="internalValue"
    :disabled="disabled"
    @update:model-value="(value) => handleSelect(value as string)"
    @update:open="handleOpenChange"
  >
    <SelectTrigger :class="cn('w-64', props.class)" v-bind="$attrs">
      <SelectValue>
        <span class="truncate">{{ displayLabel }}</span>
      </SelectValue>
    </SelectTrigger>

    <SelectContent>
      <!-- 选项：自动选择 -->
      <SelectItem :value="AUTO_VALUE">
        <div class="flex items-center gap-2">
          <MonitorSmartphone class="size-4 shrink-0 text-muted-foreground" />
          <span>{{ t('settings.adbMode.targetDevice.auto') }}</span>
        </div>
      </SelectItem>

      <div v-if="isLoading" class="px-3 py-3 text-center text-xs text-muted-foreground">
        {{ t('settings.adbMode.targetDevice.loading') }}
      </div>

      <div v-else-if="loadFailed" class="px-3 py-2.5 text-center text-xs text-destructive">
        {{ t('settings.adbMode.targetDevice.loadFailed') }}
      </div>

      <div
        v-else-if="devices.length === 0"
        class="px-3 py-3 text-center text-xs text-muted-foreground"
      >
        {{ t('settings.adbMode.targetDevice.empty') }}
      </div>

      <template v-else>
        <SelectItem v-for="device in devices" :key="device.serial" :value="device.serial">
          <div class="flex w-full items-center justify-between gap-3">
            <div class="flex min-w-0 items-center gap-2">
              <component
                :is="device.isEmulator ? MonitorSmartphone : Smartphone"
                class="size-4 shrink-0 text-muted-foreground"
              />
              <div class="flex flex-col truncate">
                <span class="truncate font-medium text-foreground">{{
                  getDeviceDisplayName(device)
                }}</span>
                <span class="font-mono text-[11px] text-muted-foreground">{{ device.serial }}</span>
              </div>
            </div>

            <!-- 设备状态指示 -->
            <span
              v-if="device.state === 'unauthorized'"
              class="shrink-0 rounded bg-amber-500/10 px-1.5 py-0.5 text-[10px] text-amber-500"
              :title="t('settings.adbMode.targetDevice.unauthorized')"
            >
              {{ t('settings.adbMode.targetDevice.unauthorized') }}
            </span>
            <span
              v-else-if="device.state !== 'device'"
              class="shrink-0 rounded bg-muted px-1.5 py-0.5 text-[10px] text-muted-foreground"
            >
              {{ t('settings.adbMode.targetDevice.offline') }}
            </span>
          </div>
        </SelectItem>
      </template>
    </SelectContent>
  </Select>
</template>
