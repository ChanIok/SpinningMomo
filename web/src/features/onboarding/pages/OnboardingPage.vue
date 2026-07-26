<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { Check, ChevronRight } from 'lucide-vue-next'
import momoOutlineSvg from '@/assets/momo-outline.svg?raw'
import zongziMomoSvg from '@/assets/zongzi-momo.svg?raw'
import { Button } from '@/components/ui/button'
import WindowTitleInput from '@/components/WindowTitleInput.vue'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { ScrollArea } from '@/components/ui/scroll-area'
import { useSettingsStore } from '@/features/settings/store'
import {
  CURRENT_ONBOARDING_FLOW_VERSION,
  DARK_FLOATING_WINDOW_COLORS,
  LIGHT_FLOATING_WINDOW_COLORS,
  type AppSettings,
  type FloatingWindowThemeMode,
} from '@/features/settings/types'
import { applyAppearanceToDocument, DEFAULT_PRIMARY_COLOR } from '@/features/settings/appearance'
import { OVERLAY_PALETTE_PRESETS } from '@/features/settings/overlayPalette'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { onboardingApi } from '../api'

type Step = 1 | 2 | 3 | 4

const store = useSettingsStore()
const router = useRouter()
const { t, setLocale } = useI18n()
const { toast } = useToast()

const step = ref<Step>(1)
const direction = ref<'forward' | 'backward'>('forward')
const isSubmitting = ref(false)

const language = ref<string>(store.appSettings.app.language.current)
const getSystemTheme = (): 'light' | 'dark' =>
  typeof window !== 'undefined' && window.matchMedia('(prefers-color-scheme: dark)').matches
    ? 'dark'
    : 'light'

const themeMode = ref<'light' | 'dark'>(getSystemTheme())
const targetTitle = ref<string>(store.appSettings.window.targetTitle || '')
const configuredInfinityNikkiGameDir = ref<string>(
  store.appSettings.extensions.infinityNikki.gameDir
)
const resolvedInfinityNikkiGameDir = ref<string | null>(null)

const stepTransitionName = computed(() =>
  direction.value === 'forward' ? 'ob-step-forward' : 'ob-step-backward'
)
const getDefaultTargetTitle = () => t('onboarding.step2.defaultTargetTitle')
const isDefaultInfinityNikkiTargetTitle = (title: string) =>
  title === '无限暖暖  ' || title === 'Infinity Nikki  '
const isInfinityNikkiUser = computed(() => resolvedInfinityNikkiGameDir.value !== null)

const DOWNLOAD_SOURCE_TEMPLATES = {
  github: 'https://github.com/ChanIok/SpinningMomo/releases/download/v{0}/{1}',
  cnb: 'https://cnb.cool/infinitymomo/SpinningMomo/-/releases/download/v{0}/{1}',
  mirror: 'https://r2.infinitymomo.com/releases/v{0}/{1}',
} as const

const getOnboardingDownloadSources = (lang: string) => {
  if (lang !== 'zh-CN') {
    return store.appSettings.update.downloadSources
  }

  return [
    { name: 'CNB', urlTemplate: DOWNLOAD_SOURCE_TEMPLATES.cnb },
    { name: 'Mirror', urlTemplate: DOWNLOAD_SOURCE_TEMPLATES.mirror },
    { name: 'GitHub', urlTemplate: DOWNLOAD_SOURCE_TEMPLATES.github },
  ]
}

const getDefaultOverlayColorsByTheme = (mode: 'light' | 'dark'): string[] => {
  const firstPreset = OVERLAY_PALETTE_PRESETS.find((preset) => preset.themeMode === mode)

  if (!firstPreset) {
    return [...store.appSettings.ui.background.overlayColors]
  }

  return firstPreset.colors.slice(0, firstPreset.mode)
}

const getDefaultPrimaryColorByTheme = (mode: 'light' | 'dark'): string =>
  DEFAULT_PRIMARY_COLOR[mode]

const getFloatingWindowThemeByWebTheme = (mode: 'light' | 'dark'): FloatingWindowThemeMode => mode

const getFloatingWindowColorsByWebTheme = (mode: 'light' | 'dark') => {
  const floatingWindowTheme = getFloatingWindowThemeByWebTheme(mode)
  return floatingWindowTheme === 'light'
    ? LIGHT_FLOATING_WINDOW_COLORS
    : DARK_FLOATING_WINDOW_COLORS
}

const syncPreviewAppearance = () => {
  const previewSettings: AppSettings = {
    ...store.appSettings,
    ui: {
      ...store.appSettings.ui,
      webTheme: {
        ...store.appSettings.ui.webTheme,
        mode: themeMode.value,
      },
      background: {
        ...store.appSettings.ui.background,
        overlayColors: getDefaultOverlayColorsByTheme(themeMode.value),
        primaryColor: getDefaultPrimaryColorByTheme(themeMode.value),
        overlayOpacity: 1,
      },
    },
  }
  applyAppearanceToDocument(previewSettings)
}

watch(
  () => language.value,
  async (nextLanguage) => {
    await setLocale(nextLanguage as 'zh-CN' | 'en-US')

    if (isInfinityNikkiUser.value && isDefaultInfinityNikkiTargetTitle(targetTitle.value)) {
      targetTitle.value = getDefaultTargetTitle()
    }
  }
)

watch(
  () => themeMode.value,
  () => {
    syncPreviewAppearance()
  },
  { immediate: true }
)

const buildInfinityNikkiExePath = (dir: string) => {
  const base = dir.replace(/\\/g, '/').replace(/\/+$/, '')
  return `${base}/InfinityNikki.exe`
}

const normalizeDirectoryPath = (dir: string) => dir.trim().replace(/[\\/]+$/, '')

const resolveInfinityNikkiAlbumPath = (dir: string): string => {
  const base = normalizeDirectoryPath(dir).replace(/\\/g, '/')
  return `${base}/X6Game/ScreenShot`
}

const isValidInfinityNikkiGameDir = async (dir: string): Promise<boolean> => {
  const normalizedDir = normalizeDirectoryPath(dir)
  if (!normalizedDir) {
    return false
  }

  const dirInfo = await onboardingApi.getFileInfo(normalizedDir)
  if (!dirInfo.exists || !dirInfo.isDirectory) {
    return false
  }

  const exeInfo = await onboardingApi.getFileInfo(buildInfinityNikkiExePath(normalizedDir))
  return exeInfo.exists && exeInfo.isRegularFile
}

const resolveConfiguredInfinityNikkiGameDir = async (): Promise<string | null> => {
  const configuredGameDir = normalizeDirectoryPath(configuredInfinityNikkiGameDir.value)
  if (!configuredGameDir) {
    return null
  }

  const isValidGameDir = await isValidInfinityNikkiGameDir(configuredGameDir)
  return isValidGameDir ? configuredGameDir : null
}

const detectInfinityNikkiGameDir = async (): Promise<string | null> => {
  try {
    const result = await onboardingApi.detectInfinityNikkiGameDirectory()
    if (!result.gameDirFound || !result.gameDir) {
      return null
    }

    const detectedGameDir = normalizeDirectoryPath(result.gameDir)
    const isValidGameDir = await isValidInfinityNikkiGameDir(detectedGameDir)
    if (!isValidGameDir) {
      return null
    }

    configuredInfinityNikkiGameDir.value = detectedGameDir
    return detectedGameDir
  } catch (error) {
    console.error('Failed to detect Infinity Nikki directory:', error)
    return null
  }
}

let infinityNikkiGameDirPromise: Promise<string | null> | null = null

const resolveInfinityNikkiGameDir = () => {
  if (!infinityNikkiGameDirPromise) {
    infinityNikkiGameDirPromise = (async () => {
      const configuredGameDir = await resolveConfiguredInfinityNikkiGameDir()
      if (configuredGameDir) {
        return configuredGameDir
      }

      return detectInfinityNikkiGameDir()
    })()
  }

  return infinityNikkiGameDirPromise
}

onMounted(() => {
  void (async () => {
    const resolvedGameDir = await resolveInfinityNikkiGameDir()
    resolvedInfinityNikkiGameDir.value = resolvedGameDir

    if (resolvedGameDir && targetTitle.value.trim() === '') {
      targetTitle.value = getDefaultTargetTitle()
    }
  })()
})

const goToNextStep = () => {
  if (step.value < 3) {
    direction.value = 'forward'
    step.value = (step.value + 1) as Step
  }
}

const goToStep = (targetStep: Step) => {
  if (targetStep < 1 || targetStep > 3 || targetStep === step.value || isSubmitting.value) {
    return
  }

  direction.value = targetStep > step.value ? 'forward' : 'backward'
  step.value = targetStep
}

const completeOnboarding = async () => {
  isSubmitting.value = true
  try {
    const nextTargetTitle = targetTitle.value.trim() === '' ? '' : targetTitle.value
    const resolvedGameDir = await resolveInfinityNikkiGameDir()
    resolvedInfinityNikkiGameDir.value = resolvedGameDir
    const externalAlbumPath = resolvedGameDir ? resolveInfinityNikkiAlbumPath(resolvedGameDir) : ''

    await store.updateSettings({
      ...store.appSettings,
      app: {
        ...store.appSettings.app,
        language: {
          current: language.value,
        },
        onboarding: {
          completed: true,
          flowVersion: CURRENT_ONBOARDING_FLOW_VERSION,
        },
      },
      ui: {
        ...store.appSettings.ui,
        floatingWindowThemeMode: getFloatingWindowThemeByWebTheme(themeMode.value),
        floatingWindowColors: getFloatingWindowColorsByWebTheme(themeMode.value),
        webTheme: {
          ...store.appSettings.ui.webTheme,
          mode: themeMode.value,
        },
        background: {
          ...store.appSettings.ui.background,
          overlayColors: getDefaultOverlayColorsByTheme(themeMode.value),
          primaryColor: getDefaultPrimaryColorByTheme(themeMode.value),
          overlayOpacity: 0.8,
        },
      },
      window: {
        ...store.appSettings.window,
        targetTitle: nextTargetTitle,
        centerLockCursor: Boolean(resolvedGameDir),
      },
      features: {
        ...store.appSettings.features,
        externalAlbumPath,
      },
      update: {
        ...store.appSettings.update,
        downloadSources: getOnboardingDownloadSources(language.value),
      },
      extensions: {
        ...store.appSettings.extensions,
        infinityNikki: {
          ...store.appSettings.extensions.infinityNikki,
          enable: Boolean(resolvedGameDir),
          gameDir: resolvedGameDir ?? '',
        },
      },
    })

    direction.value = 'forward'
    step.value = 4
  } catch (error) {
    toast.error(t('onboarding.common.saveFailed'))
    console.error('Failed to complete onboarding:', error)
  } finally {
    isSubmitting.value = false
  }
}

const enterMainInterface = async () => {
  isSubmitting.value = true
  try {
    const activatedNativeWindow = await onboardingApi.activateMainWindow()
    if (!activatedNativeWindow) {
      await router.replace('/home')
    }
  } catch (error) {
    toast.error(t('onboarding.common.enterFailed'))
    console.error('Failed to enter main interface:', error)
  } finally {
    isSubmitting.value = false
  }
}
</script>

<template>
  <div class="h-full w-full p-2 pt-0">
    <div class="surface-middle relative flex h-full w-full flex-col overflow-hidden rounded-md">
      <!-- Background Watermarks -->
      <Transition name="ob-watermark">
        <!-- Step 1 Background Watermark (Full Surface Viewport) -->
        <div
          v-if="step === 1"
          key="watermark-1"
          class="pointer-events-none absolute top-20 -right-250 bottom-0 z-0 h-[350%] w-auto max-w-full text-foreground/5 opacity-75 select-none dark:text-white/10"
          aria-hidden="true"
        >
          <div
            class="flex h-full w-full items-center justify-end [&_svg]:h-full [&_svg]:w-auto [&_svg]:max-w-none [&_svg]:shrink-0"
            v-html="momoOutlineSvg"
          ></div>
        </div>

        <!-- Step 3 Background Watermark (Full Surface Viewport) -->
        <div
          v-else-if="step === 3"
          key="watermark-3"
          class="pointer-events-none absolute -top-70 bottom-0 -left-20 z-0 h-[350%] w-auto text-foreground/5 opacity-75 select-none dark:text-white/10 [&_path]:fill-current"
          aria-hidden="true"
        >
          <div
            class="flex h-full w-full items-center justify-start [&_svg]:h-full [&_svg]:w-auto [&_svg]:max-w-none [&_svg]:shrink-0"
            v-html="zongziMomoSvg"
          ></div>
        </div>
      </Transition>

      <!-- Main Content Area -->
      <ScrollArea class="relative z-10 min-h-0 flex-1">
        <div class="relative flex min-h-full items-center justify-center px-4">
          <div class="relative z-10 flex w-full max-w-3xl flex-col justify-center py-8">
            <Transition :name="stepTransitionName" mode="out-in">
              <!-- Step 1: Start -->
              <div
                v-if="step === 1"
                key="step-1"
                class="relative z-10 flex min-h-[300px] w-full flex-col justify-center py-6 text-left"
              >
                <!-- Minimalist Content Area -->
                <div class="relative z-10 max-w-md space-y-4">
                  <div class="space-y-2">
                    <span
                      class="text-[0.65rem] font-medium tracking-[0.25em] text-primary uppercase"
                    >
                      {{ t('onboarding.step1.badge') }}
                    </span>
                    <h1 class="text-3xl font-bold tracking-tight text-foreground sm:text-4xl">
                      {{ t('onboarding.title') }}
                    </h1>
                  </div>
                  <p class="text-sm leading-relaxed text-muted-foreground">
                    {{ t('onboarding.description') }}
                  </p>

                  <!-- Action Button -->
                  <div class="pt-4">
                    <Button
                      class="h-10 gap-2 rounded-sm text-sm font-medium transition-all has-[>svg]:px-11"
                      :disabled="isSubmitting"
                      @click="goToNextStep"
                    >
                      {{ t('onboarding.actions.start') }}
                      <ChevronRight class="size-4" />
                    </Button>
                  </div>
                </div>
              </div>

              <!-- Step 2: Theme & Language -->
              <div v-else-if="step === 2" key="step-2" class="flex w-full flex-col items-center">
                <div class="space-y-3 text-center">
                  <h2 class="text-2xl font-semibold tracking-tight text-foreground">
                    {{ t('onboarding.step1.title') }}
                  </h2>
                  <p class="mx-auto max-w-md text-sm text-muted-foreground">
                    {{ t('onboarding.step1.description') }}
                  </p>
                </div>

                <!-- Theme Selection Cards -->
                <div class="ob-step2-theme-cards flex justify-center gap-5 pt-10">
                  <!-- Light Theme -->
                  <button
                    class="relative flex cursor-default flex-col items-center gap-2 rounded-md border-2 p-2 transition-all duration-200 outline-none hover:border-primary/60"
                    :class="
                      themeMode === 'light'
                        ? 'border-primary bg-background'
                        : 'border-transparent bg-background/50 hover:bg-background/80'
                    "
                    @click="themeMode = 'light'"
                  >
                    <div
                      class="flex h-24 w-40 flex-col overflow-hidden rounded-md border border-[#EADFCF] bg-[#F9F1E4]"
                    >
                      <div class="h-4 border-b border-[#EADFCF] bg-[#F1E6D5]"></div>
                      <div class="flex flex-1 gap-2 p-2">
                        <div class="w-6 flex-shrink-0 rounded-sm bg-[#F1E6D5]"></div>
                        <div class="mt-0.5 flex flex-1 flex-col gap-1">
                          <div class="h-1 w-1/3 rounded-full bg-[#DCCBB2]"></div>
                          <div class="h-1 w-full rounded-full bg-[#DCCBB2]"></div>
                          <div class="h-1 w-4/5 rounded-full bg-[#DCCBB2]"></div>
                          <div class="mt-1 h-1 w-2/3 rounded-full bg-[#DCCBB2]"></div>
                        </div>
                      </div>
                    </div>
                    <span class="text-sm font-medium text-foreground">{{
                      t('settings.appearance.theme.light')
                    }}</span>
                    <div
                      v-if="themeMode === 'light'"
                      class="absolute -top-3 -right-3 flex h-7 w-7 items-center justify-center rounded-full border-2 border-background bg-primary text-primary-foreground"
                    >
                      <svg
                        width="14"
                        height="14"
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="3"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      >
                        <polyline points="20 6 9 17 4 12"></polyline>
                      </svg>
                    </div>
                  </button>

                  <!-- Dark Theme -->
                  <button
                    class="relative flex cursor-default flex-col items-center gap-2 rounded-md border-2 p-2 transition-all duration-200 outline-none hover:border-primary/60"
                    :class="
                      themeMode === 'dark'
                        ? 'border-primary bg-background'
                        : 'border-transparent bg-background/50 hover:bg-background/80'
                    "
                    @click="themeMode = 'dark'"
                  >
                    <div
                      class="flex h-24 w-40 flex-col overflow-hidden rounded-md border border-[#2A3240] bg-[#171B22]"
                    >
                      <div class="h-4 border-b border-[#2A3240] bg-[#222834]"></div>
                      <div class="flex flex-1 gap-2 p-2">
                        <div class="w-6 flex-shrink-0 rounded-sm bg-[#222834]"></div>
                        <div class="mt-0.5 flex flex-1 flex-col gap-1">
                          <div class="h-1 w-1/3 rounded-full bg-[#3A4353]"></div>
                          <div class="h-1 w-full rounded-full bg-[#3A4353]"></div>
                          <div class="h-1 w-4/5 rounded-full bg-[#3A4353]"></div>
                          <div class="mt-1 h-1 w-2/3 rounded-full bg-[#3A4353]"></div>
                        </div>
                      </div>
                    </div>
                    <span class="text-sm font-medium text-foreground">{{
                      t('settings.appearance.theme.dark')
                    }}</span>
                    <div
                      v-if="themeMode === 'dark'"
                      class="absolute -top-3 -right-3 flex h-7 w-7 items-center justify-center rounded-full border-2 border-background bg-primary text-primary-foreground"
                    >
                      <svg
                        width="14"
                        height="14"
                        viewBox="0 0 24 24"
                        fill="none"
                        stroke="currentColor"
                        stroke-width="3"
                        stroke-linecap="round"
                        stroke-linejoin="round"
                      >
                        <polyline points="20 6 9 17 4 12"></polyline>
                      </svg>
                    </div>
                  </button>
                </div>

                <!-- Divider & Language -->
                <div class="w-full max-w-sm">
                  <div class="ob-step2-divider relative flex w-full items-center py-6">
                    <div class="flex-grow border-t border-border"></div>
                    <span class="mx-4 flex-shrink-0 text-sm text-muted-foreground">{{
                      t('common.languageLabelBilingual')
                    }}</span>
                    <div class="flex-grow border-t border-border"></div>
                  </div>

                  <Select
                    :model-value="language"
                    @update:model-value="(v) => (language = String(v))"
                  >
                    <SelectTrigger
                      class="surface-top h-11 w-full cursor-default justify-center gap-2 rounded-sm text-center"
                    >
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent class="rounded-sm shadow-none">
                      <SelectItem value="zh-CN">{{ t('common.languageZhCn') }}</SelectItem>
                      <SelectItem value="en-US">{{ t('common.languageEnUs') }}</SelectItem>
                    </SelectContent>
                  </Select>
                </div>

                <div
                  class="ob-step2-action mx-auto mt-16 flex w-full max-w-sm items-center justify-center"
                >
                  <Button
                    class="h-10 w-64 rounded-sm text-sm"
                    :disabled="isSubmitting"
                    @click="goToNextStep"
                  >
                    {{ t('onboarding.actions.next') }}
                  </Button>
                </div>
              </div>

              <!-- Step 3: Target Window -->
              <div
                v-else-if="step === 3"
                key="step-3"
                class="relative z-10 flex min-h-[300px] w-full flex-col items-end justify-center py-6 text-right"
              >
                <!-- Minimalist Content Area (Right Aligned) -->
                <div class="relative z-10 max-w-md space-y-4 text-right">
                  <div class="space-y-2">
                    <span
                      class="text-[0.65rem] font-medium tracking-[0.25em] text-primary uppercase"
                    >
                      {{ t('onboarding.step2.badge') }}
                    </span>
                    <h2 class="text-3xl font-bold tracking-tight text-foreground sm:text-4xl">
                      {{ t('onboarding.step2.title') }}
                    </h2>
                  </div>
                  <p class="text-sm leading-relaxed text-muted-foreground">
                    {{ t('onboarding.step2.description') }}
                  </p>

                  <!-- Input -->
                  <div class="ml-auto w-full pt-2">
                    <WindowTitleInput
                      v-model="targetTitle"
                      :placeholder="t('onboarding.step2.targetTitlePlaceholder')"
                      popover-no-shadow
                      input-class="h-10 text-left px-3.5"
                    />
                  </div>

                  <!-- Action Button -->
                  <div class="flex justify-end pt-4">
                    <Button
                      class="h-10 gap-2 rounded-sm text-sm font-medium transition-all has-[>svg]:px-11"
                      :disabled="isSubmitting"
                      @click="completeOnboarding"
                    >
                      {{
                        isSubmitting
                          ? t('onboarding.actions.completing')
                          : t('onboarding.actions.complete')
                      }}
                      <ChevronRight class="size-4" />
                    </Button>
                  </div>
                </div>
              </div>

              <!-- Step 4: Completed -->
              <div
                v-else-if="step === 4"
                key="step-4"
                class="flex flex-col items-center justify-center space-y-5 py-12 text-center"
              >
                <div
                  class="mb-4 flex h-20 w-20 items-center justify-center rounded-full bg-emerald-500/10 text-emerald-500"
                >
                  <Check class="size-10" />
                </div>
                <h2 class="text-3xl font-medium text-emerald-600 dark:text-emerald-400">
                  {{ t('onboarding.completed.title') }}
                </h2>
                <p class="max-w-md text-base text-muted-foreground">
                  {{ t('onboarding.completed.description') }}
                </p>
                <Button
                  class="h-10 min-w-48 rounded-sm px-6 text-sm"
                  :disabled="isSubmitting"
                  @click="enterMainInterface"
                >
                  {{
                    isSubmitting
                      ? t('onboarding.actions.enteringMain')
                      : t('onboarding.actions.enterMain')
                  }}
                </Button>
              </div>
            </Transition>
          </div>
        </div>
      </ScrollArea>

      <!-- Footer Actions & Pagination -->
      <div v-if="step < 4" class="flex w-full shrink-0 items-center justify-center px-4 pt-4 pb-8">
        <!-- Pagination Dots -->
        <div class="flex items-center gap-3">
          <button
            type="button"
            class="h-2 cursor-pointer rounded-full transition-all duration-300 disabled:cursor-not-allowed"
            :class="step === 1 ? 'w-6 bg-primary' : 'w-2 bg-primary/20 hover:bg-primary/40'"
            :disabled="isSubmitting"
            :aria-label="t('onboarding.actions.stepLabel', { step: 1 })"
            @click="goToStep(1)"
          ></button>
          <button
            type="button"
            class="h-2 cursor-pointer rounded-full transition-all duration-300 disabled:cursor-not-allowed"
            :class="step === 2 ? 'w-6 bg-primary' : 'w-2 bg-primary/20 hover:bg-primary/40'"
            :disabled="isSubmitting"
            :aria-label="t('onboarding.actions.stepLabel', { step: 2 })"
            @click="goToStep(2)"
          ></button>
          <button
            type="button"
            class="h-2 cursor-pointer rounded-full transition-all duration-300 disabled:cursor-not-allowed"
            :class="step === 3 ? 'w-6 bg-primary' : 'w-2 bg-primary/20 hover:bg-primary/40'"
            :disabled="isSubmitting"
            :aria-label="t('onboarding.actions.stepLabel', { step: 3 })"
            @click="goToStep(3)"
          ></button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 步骤切换：前进方向，旧内容向左滑出，新内容从右滑入 */
.ob-step-forward-enter-active,
.ob-step-forward-leave-active {
  transition:
    opacity 0.25s ease,
    transform 0.25s ease;
}

.ob-step-forward-enter-from {
  opacity: 0;
  transform: translateX(30px);
}

.ob-step-forward-leave-to {
  opacity: 0;
  transform: translateX(-30px);
}

/* 步骤切换：后退方向，旧内容向右滑出，新内容从左滑入 */
.ob-step-backward-enter-active,
.ob-step-backward-leave-active {
  transition:
    opacity 0.25s ease,
    transform 0.25s ease;
}

.ob-step-backward-enter-from {
  opacity: 0;
  transform: translateX(-30px);
}

.ob-step-backward-leave-to {
  opacity: 0;
  transform: translateX(30px);
}

/* 背景水印过渡：柔和淡入淡出与微缩放 */
.ob-watermark-enter-active,
.ob-watermark-leave-active {
  transition:
    opacity 0.4s cubic-bezier(0.16, 1, 0.3, 1),
    transform 0.4s cubic-bezier(0.16, 1, 0.3, 1);
}

.ob-watermark-enter-from,
.ob-watermark-leave-to {
  opacity: 0;
  transform: scale(0.96);
}

:deep([data-slot='scroll-area-viewport'] > div) {
  height: 100%;
}

@media (max-height: 640px) {
  .ob-step2-theme-cards {
    padding-top: 1.5rem; /* pt-6 */
  }

  .ob-step2-divider {
    padding-top: 1rem; /* py-4 */
    padding-bottom: 1rem;
  }

  .ob-step2-action {
    margin-top: 2rem; /* mt-8 */
  }
}
</style>
