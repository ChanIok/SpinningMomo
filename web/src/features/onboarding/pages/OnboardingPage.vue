<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { ScrollArea } from '@/components/ui/scroll-area'
import WindowTitlePickerButton from '@/components/WindowTitlePickerButton.vue'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { useSettingsStore } from '@/features/settings/store'
import {
  CURRENT_ONBOARDING_FLOW_VERSION,
  DARK_FLOATING_WINDOW_COLORS,
  LIGHT_FLOATING_WINDOW_COLORS,
  type AppSettings,
  type FloatingWindowThemeMode,
  type WebThemeMode,
} from '@/features/settings/types'
import { applyAppearanceToDocument } from '@/features/settings/appearance'
import { OVERLAY_PALETTE_PRESETS } from '@/features/settings/overlayPalette'
import { useI18n } from '@/composables/useI18n'
import { onboardingApi } from '../api'

type Step = 1 | 2 | 3

const store = useSettingsStore()
const { t, setLocale } = useI18n()

const step = ref<Step>(1)
const direction = ref<'forward' | 'backward'>('forward')
const isSubmitting = ref(false)
const stepError = ref('')

const language = ref<string>(store.appSettings.app.language.current)
const themeMode = ref<WebThemeMode>(store.appSettings.ui.webTheme.mode)
const targetTitle = ref<string>(store.appSettings.window.targetTitle || '')
const configuredInfinityNikkiGameDir = ref<string>(
  store.appSettings.extensions.infinityNikki.gameDir
)
const resolvedInfinityNikkiGameDir = ref<string | null>(null)

const isFirstStep = computed(() => step.value === 1)
const isLastStep = computed(() => step.value === 2)
const stepTransitionName = computed(() =>
  direction.value === 'forward' ? 'ob-step-forward' : 'ob-step-backward'
)
const getDefaultTargetTitle = (lang: string) =>
  lang === 'en-US' ? 'Infinity Nikki  ' : '无限暖暖  '
const isDefaultInfinityNikkiTargetTitle = (title: string) =>
  title === '无限暖暖  ' || title === 'Infinity Nikki  '
const isInfinityNikkiUser = computed(() => resolvedInfinityNikkiGameDir.value !== null)

const resolveThemePresetMode = (mode: WebThemeMode): 'light' | 'dark' => {
  if (mode === 'light' || mode === 'dark') {
    return mode
  }

  if (typeof window === 'undefined') {
    return 'dark'
  }

  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
}

const getDefaultOverlayColorsByTheme = (mode: WebThemeMode): string[] => {
  const resolvedTheme = resolveThemePresetMode(mode)
  const firstPreset = OVERLAY_PALETTE_PRESETS.find((preset) => preset.themeMode === resolvedTheme)

  if (!firstPreset) {
    return [...store.appSettings.ui.background.overlayColors]
  }

  return firstPreset.colors.slice(0, firstPreset.mode)
}

const getFloatingWindowThemeByWebTheme = (mode: WebThemeMode): FloatingWindowThemeMode => {
  return resolveThemePresetMode(mode)
}

const getFloatingWindowColorsByWebTheme = (mode: WebThemeMode) => {
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
      targetTitle.value = getDefaultTargetTitle(nextLanguage)
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
      targetTitle.value = getDefaultTargetTitle(language.value)
    }
  })()
})

const selectVisibleWindowTitle = (title: string) => {
  targetTitle.value = title
  stepError.value = ''
}

const goToNextStep = () => {
  stepError.value = ''

  if (step.value < 2) {
    direction.value = 'forward'
    step.value = (step.value + 1) as Step
  }
}

const goToPreviousStep = () => {
  stepError.value = ''
  if (step.value > 1) {
    direction.value = 'backward'
    step.value = (step.value - 1) as Step
  }
}

const completeOnboarding = async () => {
  stepError.value = ''

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
    step.value = 3
  } catch (error) {
    stepError.value = t('onboarding.common.saveFailed')
    console.error('Failed to complete onboarding:', error)
  } finally {
    isSubmitting.value = false
  }
}
</script>

<template>
  <ScrollArea class="onboarding-scroll h-full w-full">
    <div class="onboarding-shell w-full max-w-3xl">
      <div
        class="surface-middle onboarding-card flex w-full flex-col overflow-hidden rounded-md border border-border/60 shadow-sm"
      >
        <div class="shrink-0 p-8 pb-6">
          <h1 class="text-2xl font-semibold text-foreground">
            {{ t('onboarding.title') }}
          </h1>
          <p class="mt-2 text-sm text-muted-foreground">
            {{ t('onboarding.description') }}
          </p>

          <div v-if="step < 3" class="mt-8 flex items-center gap-2 text-xs">
            <div v-for="currentStep in [1, 2]" :key="currentStep" class="flex items-center gap-2">
              <div
                class="flex h-6 w-6 items-center justify-center rounded-full border"
                :class="
                  step >= currentStep
                    ? 'border-primary bg-primary text-primary-foreground'
                    : 'border-border text-muted-foreground'
                "
              >
                {{ currentStep }}
              </div>
              <div v-if="currentStep < 2" class="h-px w-10 bg-border" />
            </div>
          </div>
        </div>

        <div class="flex min-h-0 flex-1 flex-col justify-end overflow-hidden px-8 pb-6">
          <Transition :name="stepTransitionName" mode="out-in">
            <div v-if="step === 1" key="step-1" class="space-y-6">
              <div>
                <h2 class="text-lg font-medium text-foreground">
                  {{ t('onboarding.step1.title') }}
                </h2>
                <p class="mt-1 text-sm text-muted-foreground">
                  {{ t('onboarding.step1.description') }}
                </p>
              </div>

              <div class="grid gap-4 md:grid-cols-2">
                <div class="space-y-2">
                  <p class="text-sm text-foreground">{{ t('common.languageLabelBilingual') }}</p>
                  <Select
                    :model-value="language"
                    @update:model-value="(v) => (language = String(v))"
                  >
                    <SelectTrigger>
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="zh-CN">{{ t('common.languageZhCn') }}</SelectItem>
                      <SelectItem value="en-US">{{ t('common.languageEnUs') }}</SelectItem>
                    </SelectContent>
                  </Select>
                </div>

                <div class="space-y-2">
                  <p class="text-sm text-foreground">{{ t('onboarding.step1.themeLabel') }}</p>
                  <Select
                    :model-value="themeMode"
                    @update:model-value="(v) => (themeMode = v as WebThemeMode)"
                  >
                    <SelectTrigger>
                      <SelectValue />
                    </SelectTrigger>
                    <SelectContent>
                      <SelectItem value="light">{{
                        t('settings.appearance.theme.light')
                      }}</SelectItem>
                      <SelectItem value="dark">{{
                        t('settings.appearance.theme.dark')
                      }}</SelectItem>
                      <SelectItem value="system">{{
                        t('settings.appearance.theme.system')
                      }}</SelectItem>
                    </SelectContent>
                  </Select>
                </div>
              </div>
            </div>

            <div v-else-if="step === 2" key="step-2" class="space-y-6">
              <div>
                <h2 class="text-lg font-medium text-foreground">
                  {{ t('onboarding.step2.title') }}
                </h2>
                <p class="mt-1 text-sm text-muted-foreground">
                  {{ t('onboarding.step2.description') }}
                </p>
              </div>

              <div class="space-y-2">
                <p class="text-sm text-foreground">{{ t('onboarding.step2.targetTitleLabel') }}</p>
                <div class="flex gap-2">
                  <Input
                    v-model="targetTitle"
                    :placeholder="t('onboarding.step2.targetTitlePlaceholder')"
                    class="flex-1"
                  />

                  <WindowTitlePickerButton @select="selectVisibleWindowTitle" />
                </div>

                <p class="text-xs text-muted-foreground">
                  {{ t('onboarding.step2.targetTitleHint') }}
                </p>
              </div>
            </div>

            <div
              v-else-if="step === 3"
              key="step-3"
              class="flex h-full flex-col items-center justify-center space-y-6 text-center"
            >
              <h2 class="text-xl font-medium text-emerald-500">
                {{ t('onboarding.completed.title') }}
              </h2>
              <p class="max-w-md text-sm text-muted-foreground">
                {{ t('onboarding.completed.description') }}
              </p>
            </div>
          </Transition>
        </div>

        <div v-if="step < 3 || stepError" class="shrink-0 border-t border-border/60 px-8 py-6">
          <p v-if="stepError" class="text-sm text-red-500">
            {{ stepError }}
          </p>

          <div
            v-if="step < 3"
            class="flex items-center justify-between"
            :class="[stepError && 'mt-4']"
          >
            <Button
              variant="outline"
              :disabled="isFirstStep || isSubmitting"
              @click="goToPreviousStep"
            >
              {{ t('onboarding.actions.previous') }}
            </Button>

            <Button v-if="!isLastStep" :disabled="isSubmitting" @click="goToNextStep">
              {{ t('onboarding.actions.next') }}
            </Button>
            <Button v-else :disabled="isSubmitting" @click="completeOnboarding">
              {{
                isSubmitting ? t('onboarding.actions.completing') : t('onboarding.actions.complete')
              }}
            </Button>
          </div>
        </div>
      </div>
    </div>
  </ScrollArea>
</template>

<style scoped>
.onboarding-scroll :deep([data-slot='scroll-area-viewport'] > *) {
  min-height: 100%;
  display: grid;
  place-items: center;
  box-sizing: border-box;
  padding: 2rem;
}

.onboarding-shell {
  transform: translateY(-20px);
}

.onboarding-card {
  height: min(28rem, calc(100dvh - 7rem));
}

/* 步骤切换：前进方向，旧内容向左滑出，新内容从右滑入 */
.ob-step-forward-enter-active,
.ob-step-forward-leave-active {
  transition:
    opacity 0.2s ease,
    transform 0.2s ease;
}

.ob-step-forward-enter-from {
  opacity: 0;
  transform: translateX(20px);
}

.ob-step-forward-leave-to {
  opacity: 0;
  transform: translateX(-20px);
}

/* 步骤切换：后退方向，旧内容向右滑出，新内容从左滑入 */
.ob-step-backward-enter-active,
.ob-step-backward-leave-active {
  transition:
    opacity 0.2s ease,
    transform 0.2s ease;
}

.ob-step-backward-enter-from {
  opacity: 0;
  transform: translateX(-20px);
}

.ob-step-backward-leave-to {
  opacity: 0;
  transform: translateX(20px);
}
</style>
