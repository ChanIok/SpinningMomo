<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { call } from '@/core/rpc'
import { useTaskStore } from '@/core/tasks/store'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { copyToClipboard } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  Check,
  Bug,
  Mail,
  FolderOpen,
  ExternalLink,
  Globe,
  Loader2,
  Package,
  Download,
  ChevronRight,
} from 'lucide-vue-next'

interface RuntimeInfo {
  version: string
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

interface OpenLogDirectoryResult {
  success: boolean
  message: string
}

const { t, locale } = useI18n()
const { toast } = useToast()
const taskStore = useTaskStore()

const scrollAreaRef = ref<InstanceType<typeof ScrollArea>>()
const runtimeInfo = ref<RuntimeInfo | null>(null)
const currentVersionFromUpdate = ref<string | null>(null)
const isLoading = ref(false)
const error = ref<string | null>(null)
const isCheckingUpdate = ref(false)
const isStartingDownload = ref(false)
const isInstallingUpdate = ref(false)
const isOpeningLogDirectory = ref(false)
const hasUpdate = ref<boolean | null>(null)
const latestVersion = ref<string | null>(null)
const updateError = ref<string | null>(null)
const updateChecked = ref(false)
const issuesDialogOpen = ref(false)
const emailCopied = ref(false)
let emailCopiedTimer: ReturnType<typeof setTimeout> | null = null
let updateCheckedTimer: ReturnType<typeof setTimeout> | null = null

const issuesUrl = 'https://github.com/ChanIok/SpinningMomo/issues'
const feedbackEmail = 'hello@infinitymomo.com'
const licenseUrl = 'https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE'
const legalNoticeZhUrl = 'https://spin.infinitymomo.com/zh/about/legal'
const legalNoticeEnUrl = 'https://spin.infinitymomo.com/en/about/legal'
const creditsZhUrl = 'https://spin.infinitymomo.com/zh/about/credits'
const creditsEnUrl = 'https://spin.infinitymomo.com/en/about/credits'
const nuan5Url = 'https://NUAN5.PRO'

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

const toErrorMessage = (value: unknown): string => {
  if (value instanceof Error) {
    return value.message
  }
  return String(value)
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

const copyFeedbackEmail = async () => {
  const success = await copyToClipboard(feedbackEmail)
  if (success) {
    emailCopied.value = true
    if (emailCopiedTimer) {
      clearTimeout(emailCopiedTimer)
    }
    emailCopiedTimer = setTimeout(() => {
      emailCopied.value = false
    }, 1500)
    toast.success(t('about.toast.emailCopied', { email: feedbackEmail }))
  }
}

const openLogDirectory = async () => {
  if (isOpeningLogDirectory.value) {
    return
  }

  isOpeningLogDirectory.value = true
  try {
    const result = await call<OpenLogDirectoryResult>('file.openLogDirectory')
    if (!result.success) {
      throw new Error(result.message || t('about.toast.openLogDirectoryFailed'))
    }
  } catch (e) {
    toast.error(t('about.toast.openLogDirectoryFailed'))
  } finally {
    isOpeningLogDirectory.value = false
  }
}

onMounted(() => {
  void loadRuntimeInfo()
  void checkForUpdate(true)
})

onBeforeUnmount(() => {
  if (emailCopiedTimer) {
    clearTimeout(emailCopiedTimer)
  }
  if (updateCheckedTimer) {
    clearTimeout(updateCheckedTimer)
  }
})
</script>

<template>
  <ScrollArea class="h-full text-foreground" ref="scrollAreaRef">
    <div class="flex h-full w-full items-center justify-center py-[clamp(1.5rem,6vh,3rem)]">
      <div class="mx-auto flex w-full max-w-2xl flex-col items-center px-8">
        <!-- Header: Logo, Name, Version -->
        <div class="group mb-[clamp(1.5rem,6vh,3rem)] flex flex-col items-center">
          <!-- Spinning Logo -->
          <div class="perspective-1000 relative mb-[clamp(0.75rem,3vh,1.5rem)] h-28 w-28">
            <img
              src="/logo_192x192.png"
              alt="SpinningMomo Logo"
              class="h-full w-full object-contain transition-transform duration-[1.5s] ease-out group-hover:rotate-[360deg]"
            />
          </div>
          <h1
            class="mb-[clamp(0.25rem,1.5vh,0.75rem)] text-3xl font-bold tracking-tight text-foreground"
          >
            {{ t('app.name') }}
          </h1>
          <button
            v-if="appVersionText !== '-'"
            @click="handleUpdateAction"
            :disabled="
              isCheckingUpdate || isStartingDownload || isInstallingUpdate || isDownloadingUpdate
            "
            class="group/badge flex items-center gap-2 rounded-full border border-border/50 px-3 py-1 transition-all duration-300 disabled:opacity-80"
            :class="[
              hasUpdate || isDownloadedUpdateReady
                ? 'border-primary bg-primary text-sm text-primary-foreground shadow-sm hover:opacity-90'
                : 'bg-secondary/50 text-sm font-medium text-muted-foreground hover:border-border hover:bg-secondary',
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
              <template v-else>{{ t('about.runtime.version') }} {{ appVersionText }}</template>
            </span>
          </button>
        </div>

        <!-- Actions Card -->
        <div
          class="surface-top mb-[clamp(1.5rem,6vh,3rem)] w-full overflow-hidden rounded-md shadow-md"
        >
          <!-- Official Website Row -->
          <a
            href="https://spin.infinitymomo.com"
            target="_blank"
            rel="noopener noreferrer"
            class="group/link flex items-center justify-between border-b border-border/50 px-5 py-[clamp(0.75rem,3vh,1rem)] transition-colors hover:bg-accent/50"
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
          <button
            type="button"
            class="group/link flex w-full items-center justify-between px-5 py-[clamp(0.75rem,3vh,1rem)] text-left transition-colors hover:bg-accent/50"
            @click="issuesDialogOpen = true"
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
            <ChevronRight
              class="h-4 w-4 shrink-0 text-muted-foreground opacity-70 transition-colors group-hover/link:text-foreground group-hover/link:opacity-100"
            />
          </button>
        </div>

        <Dialog v-model:open="issuesDialogOpen">
          <DialogContent class="gap-5 sm:max-w-lg">
            <DialogHeader>
              <DialogTitle>{{ t('about.links.issues') }}</DialogTitle>
              <DialogDescription>{{ t('about.feedback.dialogDescription') }}</DialogDescription>
            </DialogHeader>

            <div class="space-y-4">
              <!-- Step 1: Reproduce -->
              <div class="flex gap-3">
                <div
                  class="mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary/10 text-xs font-bold text-primary"
                >
                  1
                </div>
                <div>
                  <p class="text-sm font-medium text-foreground">
                    {{ t('about.feedback.step1.title') }}
                  </p>
                  <p class="mt-0.5 text-[13px] leading-relaxed text-muted-foreground">
                    {{ t('about.feedback.step1.description') }}
                  </p>
                </div>
              </div>

              <!-- Step 2: Gather Logs -->
              <div class="flex gap-3">
                <div
                  class="mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary/10 text-xs font-bold text-primary"
                >
                  2
                </div>
                <div class="min-w-0">
                  <p class="text-sm font-medium text-foreground">
                    {{ t('about.feedback.step2.title') }}
                  </p>
                  <p class="mt-0.5 text-[13px] leading-relaxed text-muted-foreground">
                    {{ t('about.feedback.step2.description') }}
                  </p>
                  <div class="mt-2 flex flex-wrap gap-2">
                    <Button
                      variant="secondary"
                      size="sm"
                      class="h-7 text-xs"
                      @click="openLogDirectory"
                    >
                      <FolderOpen class="mr-1.5 h-3.5 w-3.5" />
                      {{ t('about.actions.openLogDirectory') }}
                    </Button>
                  </div>
                </div>
              </div>

              <!-- Step 3: Submit -->
              <div class="flex gap-3">
                <div
                  class="mt-0.5 flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary/10 text-xs font-bold text-primary"
                >
                  3
                </div>
                <div class="min-w-0 flex-1">
                  <p class="text-sm font-medium text-foreground">
                    {{ t('about.feedback.step3.title') }}
                  </p>
                  <p class="mt-0.5 text-[13px] leading-relaxed text-muted-foreground">
                    {{ t('about.feedback.step3.description') }}
                  </p>
                  <div class="mt-2 flex gap-2">
                    <Button as-child size="sm" class="h-7 text-xs">
                      <a :href="issuesUrl" target="_blank" rel="noopener noreferrer">
                        <ExternalLink class="mr-1.5 h-3.5 w-3.5" />
                        {{ t('about.feedback.openGithub') }}
                      </a>
                    </Button>
                    <Button
                      variant="secondary"
                      size="sm"
                      class="h-7 text-xs"
                      @click="copyFeedbackEmail"
                    >
                      <Check v-if="emailCopied" class="mr-1.5 h-3.5 w-3.5 text-green-500" />
                      <Mail v-else class="mr-1.5 h-3.5 w-3.5" />
                      {{ emailCopied ? t('about.status.copied') : t('about.feedback.copyEmail') }}
                    </Button>
                  </div>
                </div>
              </div>
            </div>
          </DialogContent>
        </Dialog>

        <!-- Footer -->
        <div
          class="flex flex-col items-center space-y-3 text-center text-[12px] text-muted-foreground"
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
            <a
              :href="nuan5Url"
              target="_blank"
              rel="noopener noreferrer"
              class="text-green-500 transition-colors hover:text-green-600 hover:underline dark:text-green-400 dark:hover:text-green-300"
            >
              {{ t('about.footer.creditLink') }} </a
            >{{ t('about.footer.creditSuffix') }}
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
    </div>
  </ScrollArea>
</template>

<style scoped>
:deep([data-slot='scroll-area-viewport'] > div) {
  height: 100%;
}
</style>
