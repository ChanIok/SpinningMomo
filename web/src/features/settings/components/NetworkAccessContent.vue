<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { storeToRefs } from 'pinia'
import QRCode from 'qrcode'
import { Copy } from '@lucide/vue'
import { Button } from '@/components/ui/button'
import { Switch } from '@/components/ui/switch'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Item, ItemActions, ItemContent, ItemDescription, ItemTitle } from '@/components/ui/item'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useSettingsStore } from '../store'
import { settingsApi, type LanAccessInfo } from '../api'
import { on, off } from '@/core/rpc'

const store = useSettingsStore()
const { appSettings, isInitialized } = storeToRefs(store)
const { t } = useI18n()
const { toast } = useToast()

// 设置页状态只保存后端返回的运行快照，令牌本身不写入前端持久化状态。
const info = ref<LanAccessInfo | null>(null)
const isLoading = ref(false)
const isResetting = ref(false)
const qrDataUrl = ref('')

// 同一适配器可能有多个 IPv4 地址，选择器只展示一次适配器。
const adapterOptions = computed(() => {
  const seen = new Set<string>()
  return (info.value?.addresses ?? []).filter((address) => {
    if (seen.has(address.adapterId)) return false
    seen.add(address.adapterId)
    return true
  })
})

const selectedAdapter = computed(
  () => appSettings.value.app.lanAccess.preferredAdapterId || '__auto__'
)

// 刷新服务状态、网卡地址和当前可分享链接。
const refreshInfo = async () => {
  isLoading.value = true
  try {
    info.value = await settingsApi.getLanAccessInfo()
  } catch (error) {
    toast.error(t('settings.networkAccess.error.title'), {
      description: error instanceof Error ? error.message : String(error),
    })
  } finally {
    isLoading.value = false
  }
}

// 保存 LAN 开关并刷新监听范围和链接状态。
const updateEnabled = async (enabled: boolean) => {
  try {
    await store.updateSettings({
      app: {
        ...appSettings.value.app,
        lanAccess: {
          ...appSettings.value.app.lanAccess,
          enabled,
        },
      },
    })
    await refreshInfo()
  } catch (error) {
    toast.error(t('settings.networkAccess.error.title'), {
      description: error instanceof Error ? error.message : String(error),
    })
  }
}

// 保存用户指定的首选网卡，并重新计算首选访问地址。
const updatePreferredAdapter = async (value: string) => {
  try {
    await store.updateSettings({
      app: {
        ...appSettings.value.app,
        lanAccess: {
          ...appSettings.value.app.lanAccess,
          preferredAdapterId: value === '__auto__' ? '' : value,
        },
      },
    })
    await refreshInfo()
  } catch (error) {
    toast.error(t('settings.networkAccess.error.title'), {
      description: error instanceof Error ? error.message : String(error),
    })
  }
}

// 复制已经由后端生成的完整访问链接。
const copyUrl = async (url: string) => {
  try {
    await navigator.clipboard.writeText(url)
    toast.success(t('settings.networkAccess.copy.success'))
  } catch (error) {
    toast.error(t('settings.networkAccess.copy.failed'), {
      description: error instanceof Error ? error.message : String(error),
    })
  }
}

// 轮换令牌并刷新所有网卡对应的二维码和链接。
const resetToken = async () => {
  isResetting.value = true
  try {
    info.value = await settingsApi.resetLanAccessToken()
    toast.success(t('settings.networkAccess.reset.success'))
  } catch (error) {
    toast.error(t('settings.networkAccess.error.title'), {
      description: error instanceof Error ? error.message : String(error),
    })
  } finally {
    isResetting.value = false
  }
}

// 将首选访问链接转换成二维码图片，空链接时清除旧二维码。
const updateQrCode = async (url: string) => {
  if (!url) {
    qrDataUrl.value = ''
    return
  }
  try {
    qrDataUrl.value = await QRCode.toDataURL(url, {
      width: 240,
      margin: 2,
      errorCorrectionLevel: 'M',
    })
  } catch (error) {
    qrDataUrl.value = ''
    console.error('Failed to generate LAN access QR code:', error)
  }
}

// 链接变化时同步更新二维码，避免二维码继续指向旧令牌。
watch(
  () => info.value?.preferredUrl,
  (url) => {
    void updateQrCode(url ?? '')
  },
  { immediate: true }
)

const settingsChangedHandler = () => {
  void refreshInfo()
}

// 首次进入设置页时读取当前服务状态。
onMounted(() => {
  on('settings.changed', settingsChangedHandler)
  void refreshInfo()
})

onUnmounted(() => {
  off('settings.changed', settingsChangedHandler)
})
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.loading') }}</p>
    </div>
  </div>

  <div v-else class="w-full">
    <div class="space-y-8">
      <div>
        <h3 class="text-lg font-semibold text-foreground">
          {{ t('settings.networkAccess.title') }}
        </h3>
        <p class="mt-1 text-sm text-muted-foreground">
          {{ t('settings.networkAccess.description') }}
        </p>
      </div>

      <!-- 开关变化后由后端在 HTTP 线程内立即切换监听范围。 -->
      <Item variant="surface" size="sm">
        <ItemContent>
          <ItemTitle>{{ t('settings.networkAccess.enabled.label') }}</ItemTitle>
          <ItemDescription>
            {{ t('settings.networkAccess.enabled.description') }}
            <span v-if="info?.restartRequired" class="mt-1 block text-xs text-amber-600">
              {{ t('settings.networkAccess.restartRequired') }}
            </span>
          </ItemDescription>
        </ItemContent>
        <ItemActions>
          <Switch
            :model-value="appSettings.app.lanAccess.enabled"
            @update:model-value="(value) => updateEnabled(Boolean(value))"
          />
        </ItemActions>
      </Item>

      <!-- 只有服务开启且可访问时，才展示访问链接与二维码小节 -->
      <div v-if="info?.preferredUrl" class="space-y-4">
        <div>
          <h4 class="font-medium text-foreground">{{ t('settings.networkAccess.link.title') }}</h4>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.networkAccess.link.description') }}
          </p>
        </div>

        <div
          class="flex flex-col gap-4 rounded-lg border border-border/50 bg-card/50 p-4 sm:flex-row sm:items-start"
        >
          <img
            v-if="qrDataUrl"
            :src="qrDataUrl"
            :alt="t('settings.networkAccess.qr.alt')"
            class="h-36 w-36 shrink-0 rounded border border-border/30 bg-white p-2"
          />
          <div class="min-w-0 flex-1 space-y-3">
            <!-- 融合后的网卡选择器行 -->
            <div class="flex items-center justify-between gap-2">
              <span class="text-xs font-medium text-muted-foreground">
                {{ t('settings.networkAccess.adapter.label') }}
              </span>
              <Select
                :model-value="selectedAdapter"
                :disabled="isLoading"
                @update:model-value="(value) => updatePreferredAdapter(String(value))"
              >
                <SelectTrigger class="h-8 w-52 text-xs">
                  <SelectValue :placeholder="t('settings.networkAccess.adapter.auto')" />
                </SelectTrigger>
                <SelectContent>
                  <SelectItem value="__auto__" class="text-xs">
                    {{ t('settings.networkAccess.adapter.auto') }}
                  </SelectItem>
                  <SelectItem
                    v-for="address in adapterOptions"
                    :key="address.adapterId"
                    :value="address.adapterId"
                    class="text-xs"
                  >
                    {{ address.adapterName || address.ip }}
                  </SelectItem>
                </SelectContent>
              </Select>
            </div>

            <!-- 访问链接展示框 -->
            <p
              class="rounded border border-border/30 bg-muted/50 px-3 py-2.5 font-mono text-xs break-all text-foreground"
            >
              {{ info.preferredUrl }}
            </p>

            <!-- 操作按钮组合 -->
            <div class="flex flex-wrap gap-2 pt-0.5">
              <Button variant="outline" size="sm" @click="copyUrl(info.preferredUrl)">
                <Copy class="mr-1.5 h-3.5 w-3.5" />
                {{ t('settings.networkAccess.copy.button') }}
              </Button>
              <Button variant="outline" size="sm" :disabled="isResetting" @click="resetToken">
                {{ t('settings.networkAccess.reset.button') }}
              </Button>
            </div>
          </div>
        </div>
      </div>

      <p class="text-xs leading-5 text-muted-foreground">
        {{ t('settings.networkAccess.securityNotice') }}
      </p>
    </div>
  </div>
</template>
