<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { call } from '@/core/rpc'
import { useTaskStore } from '@/core/tasks/store'
import { getCurrentEnvironment } from '@/core/env'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { copyToClipboard } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  Info,
  Monitor,
  Settings,
  Check,
  Bug,
  Heart,
  FolderOpen,
  ExternalLink,
  Globe,
} from 'lucide-vue-next'

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

interface CheckUpdateResult {
  hasUpdate: boolean
  latestVersion: string
  currentVersion: string
}

interface StartDownloadUpdateResult {
  taskId: string
  status: 'started' | 'already_running'
}

interface OpenAppDataDirectoryResult {
  success: boolean
  message: string
}

const { t, locale } = useI18n()
const { toast } = useToast()
const taskStore = useTaskStore()

const runtimeInfo = ref<RuntimeInfo | null>(null)
const currentVersionFromUpdate = ref<string | null>(null)
const isLoading = ref(false)
const error = ref<string | null>(null)
const isCheckingUpdate = ref(false)
const isStartingDownload = ref(false)
const isInstallingUpdate = ref(false)
const isOpeningAppDataDirectory = ref(false)
const hasUpdate = ref<boolean | null>(null)
const latestVersion = ref<string | null>(null)
const updateError = ref<string | null>(null)
const updateChecked = ref(false)
const showAdvanced = ref(false)
const copied = ref(false)
const environment = getCurrentEnvironment()

let copiedTimer: ReturnType<typeof setTimeout> | null = null
let updateCheckedTimer: ReturnType<typeof setTimeout> | null = null

const issuesUrl = 'https://github.com/ChanIok/SpinningMomo/issues'
const licenseUrl = 'https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE'
const legalNoticeZhUrl = 'https://spin.infinitymomo.com/zh/about/legal'
const legalNoticeEnUrl = 'https://spin.infinitymomo.com/en/about/legal'
const creditsZhUrl = 'https://spin.infinitymomo.com/zh/about/credits'
const creditsEnUrl = 'https://spin.infinitymomo.com/en/about/credits'

const legalNoticeUrl = computed(() =>
  locale.value === 'en-US' ? legalNoticeEnUrl : legalNoticeZhUrl
)

const creditsUrl = computed(() => (locale.value === 'en-US' ? creditsEnUrl : creditsZhUrl))

const appVersionText = computed(
  () => runtimeInfo.value?.version || currentVersionFromUpdate.value || '-'
)

const currentUpdateTask = computed(() => {
  return taskStore.tasks.find((task) => task.type === 'update.download') ?? null
})

const isDownloadingUpdate = computed(() => {
  const status = currentUpdateTask.value?.status
  return status === 'queued' || status === 'running'
})

const isDownloadedUpdateReady = computed(
  () => currentUpdateTask.value?.status === 'succeeded' && hasUpdate.value !== false
)

const environmentText = computed(() => {
  return environment === 'webview'
    ? t('about.runtime.environmentWebview')
    : t('about.runtime.environmentWeb')
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

const checkForUpdate = async (silent = false) => {
  if (isCheckingUpdate.value || isStartingDownload.value || isInstallingUpdate.value) {
    return
  }

  isCheckingUpdate.value = true
  updateError.value = null
  updateChecked.value = false

  if (updateCheckedTimer) {
    clearTimeout(updateCheckedTimer)
  }

  try {
    const result = await call<CheckUpdateResult>('update.check_for_update')

    hasUpdate.value = result.hasUpdate
    latestVersion.value = result.latestVersion
    currentVersionFromUpdate.value = result.currentVersion

    if (result.hasUpdate) {
      if (!silent) toast.success(t('about.toast.updateAvailable'))
    } else {
      if (!silent) {
        toast.info(t('about.toast.upToDate'))
        updateChecked.value = true
        updateCheckedTimer = setTimeout(() => {
          updateChecked.value = false
        }, 3000)
      }
    }
  } catch (e) {
    updateError.value = toErrorMessage(e)
    if (!silent) toast.error(t('about.toast.updateCheckFailed'))
  } finally {
    isCheckingUpdate.value = false
  }
}

const downloadAndInstallUpdate = async () => {
  if (isCheckingUpdate.value || isStartingDownload.value || isInstallingUpdate.value) {
    return
  }

  if (isDownloadedUpdateReady.value) {
    isInstallingUpdate.value = true
    updateError.value = null

    try {
      await call('update.install_update', { restart: true })
    } catch (e) {
      updateError.value = toErrorMessage(e)
      toast.error(t('about.toast.updateInstallFailed'))
      isInstallingUpdate.value = false
    }
    return
  }

  if (!hasUpdate.value) {
    return
  }

  isStartingDownload.value = true
  updateError.value = null

  try {
    await call<StartDownloadUpdateResult>('update.start_download')
  } catch (e) {
    updateError.value = toErrorMessage(e)
    toast.error(t('about.toast.updateDownloadFailed'))
  } finally {
    isStartingDownload.value = false
  }
}

const handleUpdateAction = async () => {
  if (hasUpdate.value || isDownloadedUpdateReady.value) {
    await downloadAndInstallUpdate()
    return
  }
  await checkForUpdate()
}

const copyDiagnostics = async () => {
  const success = await copyToClipboard(diagnosticsText.value)
  if (success) {
    markCopied()
  }
}

const openAppDataDirectory = async () => {
  if (isOpeningAppDataDirectory.value) {
    return
  }

  isOpeningAppDataDirectory.value = true
  try {
    const result = await call<OpenAppDataDirectoryResult>('file.openAppDataDirectory')
    if (!result.success) {
      throw new Error(result.message || t('about.toast.openDataDirectoryFailed'))
    }
  } catch (e) {
    toast.error(t('about.toast.openDataDirectoryFailed'))
  } finally {
    isOpeningAppDataDirectory.value = false
  }
}

onMounted(() => {
  void loadRuntimeInfo()
  void checkForUpdate(true)
})

onBeforeUnmount(() => {
  if (copiedTimer) {
    clearTimeout(copiedTimer)
  }
  if (updateCheckedTimer) {
    clearTimeout(updateCheckedTimer)
  }
})
</script>

<template>
  <ScrollArea class="h-full text-foreground">
    <div class="mx-auto flex min-h-full w-full max-w-2xl flex-col items-center px-8 pt-24 pb-12">
      <!-- Header: Logo, Name, Version -->
      <div class="group mb-12 flex flex-col items-center">
        <!-- Spinning Logo -->
        <div class="perspective-1000 relative mb-6 h-28 w-28">
          <img
            src="/logo_192x192.png"
            alt="SpinningMomo Logo"
            class="h-full w-full object-contain drop-shadow-2xl transition-transform duration-[1.5s] ease-out group-hover:rotate-[360deg]"
          />
        </div>
        <h1 class="mb-3 text-3xl font-bold tracking-tight text-foreground">{{ t('app.name') }}</h1>
        <button
          v-if="appVersionText !== '-'"
          @click="handleUpdateAction"
          :disabled="
            isCheckingUpdate || isStartingDownload || isInstallingUpdate || isDownloadingUpdate
          "
          class="group/badge flex items-center gap-2 rounded-full border border-border/50 px-3 py-1 transition-all duration-300 disabled:opacity-80"
          :class="[
            hasUpdate || isDownloadedUpdateReady
              ? 'border-primary bg-primary text-primary-foreground shadow-sm hover:opacity-90'
              : 'bg-secondary/50 text-[13px] font-medium text-muted-foreground hover:border-border hover:bg-secondary',
          ]"
        >
          <!-- Status Icon -->
          <Loader2
            v-if="
              isCheckingUpdate || isStartingDownload || isInstallingUpdate || isDownloadingUpdate
            "
            class="h-3.5 w-3.5 animate-spin"
          />
          <Package v-else-if="isDownloadedUpdateReady" class="h-3.5 w-3.5" />
          <Download v-else-if="hasUpdate" class="h-3.5 w-3.5" />
          <Check
            v-else
            class="h-3.5 w-3.5 text-green-500 transition-transform group-hover/badge:scale-110"
          />

          <!-- Status Text -->
          <span>
            <template v-if="isCheckingUpdate">{{ t('about.actions.checkingUpdate') }}</template>
            <template v-else-if="isInstallingUpdate">{{
              t('about.actions.installingUpdate')
            }}</template>
            <template v-else-if="isDownloadedUpdateReady">{{
              t('about.actions.installDownloadedUpdate', { version: latestVersion || '' })
            }}</template>
            <template v-else-if="hasUpdate">{{
              t('about.actions.downloadUpdate', { version: latestVersion || '' })
            }}</template>
            <template v-else
              >{{ t('about.runtime.version') }} {{ appVersionText }} (64-bit)</template
            >
          </span>
        </button>
      </div>

      <!-- Actions Card -->
      <div
        class="surface-top mb-12 w-full overflow-hidden rounded-md border border-border shadow-sm"
      >
        <!-- Official Website Row -->
        <a
          href="https://spin.infinitymomo.com"
          target="_blank"
          rel="noopener noreferrer"
          class="group/link flex items-center justify-between border-b border-border/50 px-5 py-4 transition-colors hover:bg-accent/50"
        >
          <div class="flex items-center gap-4">
            <div
              class="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-primary/10 text-primary"
            >
              <Globe class="h-4 w-4" />
            </div>
            <span class="text-[15px] font-medium text-card-foreground">{{
              t('about.links.officialWebsite')
            }}</span>
          </div>
          <div class="flex items-center gap-2">
            <span
              class="text-sm text-muted-foreground transition-colors group-hover/link:text-foreground"
              >spin.infinitymomo.com</span
            >
            <ExternalLink
              class="h-4 w-4 text-muted-foreground opacity-70 transition-colors group-hover/link:text-foreground group-hover/link:opacity-100"
            />
          </div>
        </a>

        <!-- Report Issues Row -->
        <a
          :href="issuesUrl"
          target="_blank"
          rel="noopener noreferrer"
          class="group/link flex items-center justify-between px-5 py-4 transition-colors hover:bg-accent/50"
        >
          <div class="flex items-center gap-4">
            <div
              class="flex h-8 w-8 shrink-0 items-center justify-center rounded-full bg-primary/10 text-primary"
            >
              <Bug class="h-4 w-4" />
            </div>
            <span class="text-[15px] font-medium text-card-foreground">{{
              t('about.links.issues')
            }}</span>
          </div>
          <ExternalLink
            class="h-4 w-4 text-muted-foreground opacity-70 transition-colors group-hover/link:text-foreground group-hover/link:opacity-100"
          />
        </a>
      </div>

      <!-- Advanced Diagnostics (Hidden by default, for support) -->
      <div class="mb-8 flex w-full flex-1 flex-col items-center text-muted-foreground/80">
        <Button
          variant="ghost"
          size="sm"
          class="h-8 rounded-full text-xs hover:bg-secondary"
          @click="showAdvanced = !showAdvanced"
        >
          <Settings class="mr-1.5 h-3.5 w-3.5 opacity-70" />
          {{ showAdvanced ? t('about.actions.hideAdvanced') : t('about.actions.showAdvanced') }}
        </Button>

        <div
          v-show="showAdvanced"
          class="mt-4 w-full max-w-sm space-y-3 rounded-xl border border-border/50 bg-secondary/20 p-4 text-xs"
        >
          <div class="flex justify-between border-b border-border/40 pb-2">
            <span class="flex items-center gap-1.5"><Monitor class="h-3.5 w-3.5" /> OS</span>
            <span class="font-medium">{{ osText }}</span>
          </div>
          <div class="flex justify-between border-b border-border/40 pb-2">
            <span class="flex items-center gap-1.5"><Heart class="h-3.5 w-3.5" /> WebView2</span>
            <span class="font-medium">{{ webview2Text }}</span>
          </div>
          <div class="flex justify-end gap-3 pt-2">
            <Button
              variant="secondary"
              size="sm"
              class="h-7 px-2 text-xs"
              @click="openAppDataDirectory"
            >
              <FolderOpen class="mr-1.5 h-3.5 w-3.5" /> {{ t('about.actions.openDataDirectory') }}
            </Button>
            <Button variant="secondary" size="sm" class="h-7 px-2 text-xs" @click="copyDiagnostics">
              <Check v-if="copied" class="mr-1.5 h-3.5 w-3.5 text-green-500" />
              <Info v-else class="mr-1.5 h-3.5 w-3.5" />
              {{ copied ? t('about.status.copied') : t('about.actions.copyDiagnostics') }}
            </Button>
          </div>
        </div>
      </div>

      <!-- Footer -->
      <div
        class="mt-auto flex flex-col items-center space-y-3 text-center text-[13px] text-muted-foreground"
      >
        <p>&copy; 2026 InfinityMomo. {{ t('about.footer.rightsReserved') }}</p>
        <p>
          {{ t('about.footer.openSourcePrefix') }}
          <a
            :href="creditsUrl"
            target="_blank"
            rel="noopener noreferrer"
            class="text-primary/80 transition-colors hover:text-primary hover:underline"
          >
            {{ t('about.footer.openSourceLink') }} </a
          >{{ t('about.footer.openSourceSuffix') }}
        </p>
        <div class="flex items-center justify-center gap-5 pt-2">
          <a
            :href="legalNoticeUrl"
            target="_blank"
            rel="noopener noreferrer"
            class="transition-colors hover:text-foreground hover:underline"
            >{{ t('about.links.legalNotice') }}</a
          >
          <a
            :href="licenseUrl"
            target="_blank"
            rel="noopener noreferrer"
            class="transition-colors hover:text-foreground hover:underline"
            >{{ t('about.links.license') }}</a
          >
        </div>
      </div>
    </div>
  </ScrollArea>
</template>
