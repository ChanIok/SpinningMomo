<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { isWebView } from '@/core/env'
import { useSettingsStore } from '@/features/settings/store'
import {
  CURRENT_ONBOARDING_FLOW_VERSION,
  type AppSettings,
  type WebThemeMode,
} from '@/features/settings/types'
import { applyAppearanceToDocument } from '@/features/settings/appearance'
import { OVERLAY_PALETTE_PRESETS } from '@/features/settings/overlayPalette'
import { useI18n } from '@/composables/useI18n'
import { onboardingApi } from '../api'

type Step = 1 | 2 | 3 | 4
type DetectionStatus = 'idle' | 'loading' | 'success' | 'failed'

const store = useSettingsStore()
const { t, setLocale } = useI18n()

const step = ref<Step>(1)
const isSubmitting = ref(false)
const stepError = ref('')

const language = ref<string>(store.appSettings.app.language.current)
const themeMode = ref<WebThemeMode>(store.appSettings.ui.webTheme.mode)
const getDefaultTargetTitle = (lang: string) =>
  lang === 'en-US' ? 'Infinity Nikki  ' : '无限暖暖  '

const gameDir = ref<string>(store.appSettings.plugins.infinityNikki.gameDir || '')
const targetTitle = ref<string>(
  store.appSettings.window.targetTitle === '无限暖暖  ' ||
    store.appSettings.window.targetTitle === 'Infinity Nikki  '
    ? getDefaultTargetTitle(store.appSettings.app.language.current)
    : store.appSettings.window.targetTitle ||
        getDefaultTargetTitle(store.appSettings.app.language.current)
)
const skipInfinityNikki = ref(false)

const hasDetectedOnce = ref(false)
const detectionStatus = ref<DetectionStatus>('idle')
const detectionMessageRaw = ref('')
const detectionMessageKey = ref('onboarding.step2.detectPending')

const showHeader = computed(() => isWebView())

const isFirstStep = computed(() => step.value === 1)
const isLastStep = computed(() => step.value === 3)

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

const syncPreviewAppearance = () => {
  const previewSettings: AppSettings = {
    ...store.appSettings,
    ui: {
      ...store.appSettings.ui,
      webTheme: {
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

    if (targetTitle.value === '无限暖暖  ' || targetTitle.value === 'Infinity Nikki  ') {
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

watch(
  () => step.value,
  (currentStep) => {
    stepError.value = ''
    if (currentStep === 2 && !skipInfinityNikki.value && !hasDetectedOnce.value) {
      void detectInfinityNikkiDirectory()
    }
  }
)

const detectionMessageClass = computed(() => {
  if (detectionStatus.value === 'success') {
    return 'text-emerald-500'
  }
  if (detectionStatus.value === 'failed') {
    return 'text-amber-500'
  }
  return 'text-muted-foreground'
})

const detectInfinityNikkiDirectory = async () => {
  detectionStatus.value = 'loading'
  detectionMessageKey.value = 'onboarding.step2.detecting'
  detectionMessageRaw.value = ''

  try {
    const result = await onboardingApi.detectInfinityNikkiGameDirectory()
    hasDetectedOnce.value = true

    if (result.gameDirFound && result.gameDir) {
      gameDir.value = result.gameDir
      detectionStatus.value = 'success'
      detectionMessageKey.value = 'onboarding.step2.detectSuccess'
      detectionMessageRaw.value = ''
      return
    }

    detectionStatus.value = 'failed'
    if (result.message) {
      detectionMessageRaw.value = result.message
    } else {
      detectionMessageKey.value = 'onboarding.step2.detectFailed'
      detectionMessageRaw.value = ''
    }
  } catch (error) {
    hasDetectedOnce.value = true
    detectionStatus.value = 'failed'
    detectionMessageKey.value = 'onboarding.step2.detectFailed'
    detectionMessageRaw.value = ''
    console.error('Failed to detect Infinity Nikki directory:', error)
  }
}

const selectGameDirectory = async () => {
  try {
    const selectedPath = await onboardingApi.selectDirectory(t('onboarding.step2.selectDirectory'))
    if (!selectedPath) {
      return
    }

    gameDir.value = selectedPath
    stepError.value = ''
    if (detectionStatus.value !== 'success') {
      detectionStatus.value = 'failed'
      detectionMessageKey.value = 'onboarding.step2.manualInputHint'
      detectionMessageRaw.value = ''
    }
  } catch (error) {
    console.error('Failed to select directory:', error)
  }
}

const toggleSkipInfinityNikki = () => {
  if (skipInfinityNikki.value) {
    skipInfinityNikki.value = false
  } else {
    skipInfinityNikki.value = true
    stepError.value = ''
    step.value = 3
  }
}

const goToNextStep = () => {
  stepError.value = ''

  if (step.value === 2 && !skipInfinityNikki.value && !gameDir.value.trim()) {
    stepError.value = t('onboarding.step2.gameDirRequired')
    return
  }

  if (step.value < 3) {
    step.value = (step.value + 1) as Step
  }
}

const goToPreviousStep = () => {
  stepError.value = ''
  if (step.value > 1) {
    step.value = (step.value - 1) as Step
  }
}

const completeOnboarding = async () => {
  stepError.value = ''

  const trimmedTargetTitle = targetTitle.value.trim()
  if (!trimmedTargetTitle) {
    stepError.value = t('onboarding.step3.targetTitleRequired')
    return
  }

  const trimmedGameDir = gameDir.value.trim()

  isSubmitting.value = true
  try {
    if (!skipInfinityNikki.value) {
      if (!trimmedGameDir) {
        step.value = 2
        stepError.value = t('onboarding.step2.gameDirRequired')
        return
      }

      const dirInfo = await onboardingApi.getFileInfo(trimmedGameDir)
      if (!dirInfo.exists || !dirInfo.isDirectory) {
        step.value = 2
        stepError.value = t('onboarding.step2.gameDirInvalid')
        return
      }
    }

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
        webTheme: {
          mode: themeMode.value,
        },
        background: {
          ...store.appSettings.ui.background,
          overlayColors: getDefaultOverlayColorsByTheme(themeMode.value),
        },
      },
      window: {
        ...store.appSettings.window,
        targetTitle: targetTitle.value,
      },
      plugins: {
        ...store.appSettings.plugins,
        infinityNikki: {
          enable: !skipInfinityNikki.value,
          gameDir: skipInfinityNikki.value ? '' : trimmedGameDir,
        },
      },
    })

    step.value = 4
  } catch (error) {
    stepError.value = t('onboarding.common.saveFailed')
    console.error('Failed to complete onboarding:', error)
  } finally {
    isSubmitting.value = false
  }
}
</script>

<template>
  <div
    class="mx-auto flex h-full w-full max-w-3xl items-center justify-center p-8"
    :class="[showHeader && 'pt-0']"
  >
    <div class="surface-middle w-full rounded-md border border-border/60 p-8 shadow-sm">
      <div class="mb-8">
        <h1 class="text-2xl font-semibold text-foreground">
          {{ t('onboarding.title') }}
        </h1>
        <p class="mt-2 text-sm text-muted-foreground">
          {{ t('onboarding.description') }}
        </p>
      </div>

      <div v-if="step < 4" class="mb-8 flex items-center gap-2 text-xs">
        <div v-for="currentStep in [1, 2, 3]" :key="currentStep" class="flex items-center gap-2">
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
          <div v-if="currentStep < 3" class="h-px w-10 bg-border" />
        </div>
      </div>

      <div v-if="step === 1" class="space-y-6">
        <div>
          <h2 class="text-lg font-medium text-foreground">{{ t('onboarding.step1.title') }}</h2>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('onboarding.step1.description') }}
          </p>
        </div>

        <div class="grid gap-4 md:grid-cols-2">
          <div class="space-y-2">
            <p class="text-sm text-foreground">{{ t('onboarding.step1.languageLabel') }}</p>
            <Select :model-value="language" @update:model-value="(v) => (language = String(v))">
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
                <SelectItem value="light">{{ t('settings.appearance.theme.light') }}</SelectItem>
                <SelectItem value="dark">{{ t('settings.appearance.theme.dark') }}</SelectItem>
                <SelectItem value="system">{{ t('settings.appearance.theme.system') }}</SelectItem>
              </SelectContent>
            </Select>
          </div>
        </div>
      </div>

      <div v-else-if="step === 2" class="space-y-6">
        <div>
          <h2 class="text-lg font-medium text-foreground">{{ t('onboarding.step2.title') }}</h2>
          <p class="mt-1 text-sm" :class="detectionMessageClass">
            {{ detectionMessageRaw || t(detectionMessageKey) }}
          </p>
        </div>

        <div class="space-y-3">
          <div class="flex gap-2">
            <Input
              v-model="gameDir"
              :placeholder="t('onboarding.step2.gameDirPlaceholder')"
              :disabled="detectionStatus === 'loading' || skipInfinityNikki"
            />
            <Button
              variant="outline"
              size="sm"
              class="shrink-0"
              :disabled="detectionStatus === 'loading' || skipInfinityNikki"
              @click="selectGameDirectory"
            >
              {{ t('onboarding.step2.selectButton') }}
            </Button>
          </div>

          <p class="text-xs text-muted-foreground">
            <button
              class="underline underline-offset-2"
              type="button"
              @click="toggleSkipInfinityNikki"
            >
              {{
                skipInfinityNikki
                  ? t('onboarding.step2.undoSkipLink')
                  : t('onboarding.step2.skipLink')
              }}
            </button>
          </p>
        </div>
      </div>

      <div v-else-if="step === 3" class="space-y-6">
        <div>
          <h2 class="text-lg font-medium text-foreground">{{ t('onboarding.step3.title') }}</h2>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('onboarding.step3.description') }}
          </p>
        </div>

        <div class="space-y-2">
          <p class="text-sm text-foreground">{{ t('onboarding.step3.targetTitleLabel') }}</p>
          <Input
            v-model="targetTitle"
            :placeholder="t('onboarding.step3.targetTitlePlaceholder')"
          />
        </div>
      </div>

      <div
        v-else-if="step === 4"
        class="flex flex-col items-center justify-center space-y-6 py-8 text-center"
      >
        <h2 class="text-xl font-medium text-emerald-500">{{ t('onboarding.completed.title') }}</h2>
        <p class="mt-4 text-sm text-muted-foreground">
          {{ t('onboarding.completed.description') }}
        </p>
      </div>

      <p v-if="stepError" class="mt-5 text-sm text-red-500">
        {{ stepError }}
      </p>

      <div v-if="step < 4" class="mt-8 flex items-center justify-between">
        <Button variant="outline" :disabled="isFirstStep || isSubmitting" @click="goToPreviousStep">
          {{ t('onboarding.actions.previous') }}
        </Button>

        <Button v-if="!isLastStep" :disabled="isSubmitting" @click="goToNextStep">
          {{ t('onboarding.actions.next') }}
        </Button>
        <Button v-else :disabled="isSubmitting" @click="completeOnboarding">
          {{ isSubmitting ? t('onboarding.actions.completing') : t('onboarding.actions.complete') }}
        </Button>
      </div>
    </div>
  </div>
</template>
