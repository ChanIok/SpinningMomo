<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { call } from '@/core/rpc'
import { getCurrentEnvironment } from '@/core/env'
import { useI18n } from '@/composables/useI18n'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Info, Monitor, Settings, Camera, TriangleAlert } from 'lucide-vue-next'

interface RuntimeInfo {
  version: string
  osName: string
  osMajorVersion: number
  osMinorVersion: number
  osBuildNumber: number
  isWebview2Available: boolean
  webview2Version: string
  isCaptureSupported: boolean
  isProcessLoopbackAudioSupported: boolean
}

const { t, locale } = useI18n()

const runtimeInfo = ref<RuntimeInfo | null>(null)
const isLoading = ref(false)
const error = ref<string | null>(null)
const showAdvanced = ref(false)
const copied = ref(false)
const environment = getCurrentEnvironment()

let copiedTimer: ReturnType<typeof setTimeout> | null = null

const docsUrl = 'https://chaniok.github.io/SpinningMomo'
const releaseUrl = 'https://github.com/ChanIok/SpinningMomo/releases/latest'
const issuesUrl = 'https://github.com/ChanIok/SpinningMomo/issues'
const licenseUrl = 'https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE'
const legalNoticeZhUrl = 'https://chaniok.github.io/SpinningMomo/zh/legal/notice'
const legalNoticeEnUrl = 'https://chaniok.github.io/SpinningMomo/en/legal/notice'
const creditsZhUrl = 'https://chaniok.github.io/SpinningMomo/zh/credits'
const creditsEnUrl = 'https://chaniok.github.io/SpinningMomo/en/credits'

const legalNoticeUrl = computed(() =>
  locale.value === 'en-US' ? legalNoticeEnUrl : legalNoticeZhUrl
)

const creditsUrl = computed(() => (locale.value === 'en-US' ? creditsEnUrl : creditsZhUrl))

const appVersionText = computed(() => runtimeInfo.value?.version || '-')

const environmentText = computed(() => {
  const key =
    environment === 'webview' ? 'about.runtime.environmentWebview' : 'about.runtime.environmentWeb'
  return t(key)
})

const osText = computed(() => {
  if (!runtimeInfo.value) {
    return '-'
  }
  return `${runtimeInfo.value.osName} ${runtimeInfo.value.osMajorVersion}.${runtimeInfo.value.osMinorVersion}.${runtimeInfo.value.osBuildNumber}`
})

const webview2Text = computed(() => {
  if (!runtimeInfo.value) {
    return '-'
  }
  if (!runtimeInfo.value.isWebview2Available) {
    return t('about.runtime.unavailable')
  }
  return runtimeInfo.value.webview2Version || t('about.runtime.available')
})

const diagnosticsText = computed(() => {
  return [
    t('about.diagnostics.title'),
    `${t('about.runtime.version')}: ${appVersionText.value}`,
    `${t('about.runtime.environment')}: ${environmentText.value}`,
    `${t('about.runtime.os')}: ${osText.value}`,
    `${t('about.runtime.webview2')}: ${webview2Text.value}`,
    `${t('about.runtime.capture')}: ${formatCapability(runtimeInfo.value?.isCaptureSupported)}`,
    `${t('about.runtime.loopback')}: ${formatCapability(runtimeInfo.value?.isProcessLoopbackAudioSupported)}`,
  ].join('\n')
})

const toErrorMessage = (value: unknown): string => {
  if (value instanceof Error) {
    return value.message
  }
  return String(value)
}

const formatCapability = (value: boolean | undefined): string => {
  if (value === true) {
    return t('about.runtime.supported')
  }
  if (value === false) {
    return t('about.runtime.unsupported')
  }
  return '-'
}

const copyWithExecCommand = (text: string): boolean => {
  if (typeof document === 'undefined') {
    return false
  }

  const textarea = document.createElement('textarea')
  textarea.value = text
  textarea.style.position = 'fixed'
  textarea.style.opacity = '0'
  textarea.style.pointerEvents = 'none'
  document.body.appendChild(textarea)
  textarea.focus()
  textarea.select()

  let success = false
  try {
    success = document.execCommand('copy')
  } catch {
    success = false
  }

  document.body.removeChild(textarea)
  return success
}

const markCopied = () => {
  copied.value = true
  if (copiedTimer) {
    clearTimeout(copiedTimer)
  }
  copiedTimer = setTimeout(() => {
    copied.value = false
  }, 1500)
}

const loadRuntimeInfo = async () => {
  isLoading.value = true
  error.value = null
  try {
    runtimeInfo.value = await call<RuntimeInfo>('runtime_info.get')
  } catch (e) {
    error.value = toErrorMessage(e)
  } finally {
    isLoading.value = false
  }
}

const copyDiagnostics = async () => {
  const text = diagnosticsText.value
  let success = false

  if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(text)
      success = true
    } catch {
      success = false
    }
  }

  if (!success) {
    success = copyWithExecCommand(text)
  }

  if (success) {
    markCopied()
  }
}

onMounted(() => {
  void loadRuntimeInfo()
})

onBeforeUnmount(() => {
  if (copiedTimer) {
    clearTimeout(copiedTimer)
  }
})
</script>

<template>
  <ScrollArea class="h-full text-foreground">
    <div class="mx-auto w-full max-w-3xl p-8 pb-12">
      <div class="space-y-2 pr-4">
        <h1 class="text-2xl font-semibold">{{ t('about.title') }}</h1>
        <p class="text-sm text-muted-foreground">{{ t('about.description') }}</p>
      </div>

      <div class="mt-6 rounded-lg border bg-card/60 p-4 pr-5">
        <div class="flex flex-wrap items-center justify-between gap-3">
          <div>
            <p class="text-xs text-muted-foreground">{{ t('about.runtime.version') }}</p>
            <p class="text-sm font-medium">{{ appVersionText }}</p>
          </div>
          <Button variant="outline" size="sm" :disabled="isLoading" @click="loadRuntimeInfo">
            {{ isLoading ? t('about.status.loading') : t('about.actions.refresh') }}
          </Button>
        </div>
      </div>

      <div class="mt-6 pr-4">
        <p class="text-xs font-medium tracking-wide text-muted-foreground">
          {{ t('about.links.title') }}
        </p>
        <div class="mt-3 flex flex-wrap gap-2">
          <Button as-child variant="secondary" size="sm">
            <a :href="docsUrl" target="_blank" rel="noopener noreferrer">
              {{ t('about.links.docs') }}
            </a>
          </Button>
          <Button as-child variant="secondary" size="sm">
            <a :href="releaseUrl" target="_blank" rel="noopener noreferrer">
              {{ t('about.links.release') }}
            </a>
          </Button>
          <Button as-child variant="secondary" size="sm">
            <a :href="issuesUrl" target="_blank" rel="noopener noreferrer">
              {{ t('about.links.issues') }}
            </a>
          </Button>
          <Button as-child variant="secondary" size="sm">
            <a :href="licenseUrl" target="_blank" rel="noopener noreferrer">
              {{ t('about.links.license') }}
            </a>
          </Button>
          <Button as-child variant="secondary" size="sm">
            <a :href="legalNoticeUrl" target="_blank" rel="noopener noreferrer">
              {{ t('about.links.legalNotice') }}
            </a>
          </Button>
          <Button as-child variant="secondary" size="sm">
            <a :href="creditsUrl" target="_blank" rel="noopener noreferrer">
              {{ t('about.links.credits') }}
            </a>
          </Button>
        </div>
      </div>

      <div class="mt-8 pr-4">
        <Button variant="ghost" size="sm" class="px-0" @click="showAdvanced = !showAdvanced">
          {{ showAdvanced ? t('about.actions.hideAdvanced') : t('about.actions.showAdvanced') }}
        </Button>

        <div v-if="showAdvanced" class="mt-3 rounded-lg border bg-card/40 p-4">
          <div v-if="isLoading" class="text-sm text-muted-foreground">
            {{ t('about.status.loading') }}
          </div>

          <div v-else-if="error" class="space-y-3">
            <p class="text-sm text-muted-foreground">{{ t('about.status.loadFailed') }}</p>
            <p class="text-sm text-red-500">{{ error }}</p>
            <Button variant="outline" size="sm" @click="loadRuntimeInfo">
              {{ t('about.actions.retry') }}
            </Button>
          </div>

          <template v-else>
            <div class="space-y-3 text-sm">
              <div class="flex items-center justify-between gap-4">
                <span class="flex items-center gap-2 text-muted-foreground">
                  <Info class="h-4 w-4" />
                  {{ t('about.runtime.environment') }}
                </span>
                <span class="text-right font-medium">{{ environmentText }}</span>
              </div>

              <div class="flex items-center justify-between gap-4">
                <span class="flex items-center gap-2 text-muted-foreground">
                  <Monitor class="h-4 w-4" />
                  {{ t('about.runtime.os') }}
                </span>
                <span class="text-right font-medium">{{ osText }}</span>
              </div>

              <div class="flex items-center justify-between gap-4">
                <span class="flex items-center gap-2 text-muted-foreground">
                  <Settings class="h-4 w-4" />
                  {{ t('about.runtime.webview2') }}
                </span>
                <span class="text-right font-medium">{{ webview2Text }}</span>
              </div>

              <div class="flex items-center justify-between gap-4">
                <span class="flex items-center gap-2 text-muted-foreground">
                  <Camera class="h-4 w-4" />
                  {{ t('about.runtime.capture') }}
                </span>
                <span class="text-right font-medium">
                  {{ formatCapability(runtimeInfo?.isCaptureSupported) }}
                </span>
              </div>

              <div class="flex items-center justify-between gap-4">
                <span class="flex items-center gap-2 text-muted-foreground">
                  <TriangleAlert class="h-4 w-4" />
                  {{ t('about.runtime.loopback') }}
                </span>
                <span class="text-right font-medium">
                  {{ formatCapability(runtimeInfo?.isProcessLoopbackAudioSupported) }}
                </span>
              </div>
            </div>

            <div class="mt-4">
              <Button variant="outline" size="sm" @click="copyDiagnostics">
                {{ copied ? t('about.status.copied') : t('about.actions.copyDiagnostics') }}
              </Button>
            </div>
          </template>
        </div>
      </div>
    </div>
  </ScrollArea>
</template>
