<script setup lang="ts">
import { ref } from 'vue'
import { Button } from '@/components/ui/button'
import { ScanText, FolderSymlink, Sparkles, ChevronRight } from '@lucide/vue'
import zongziMomoSvg from '@/assets/zongzi-momo.svg?raw'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useSettingsStore } from '@/features/settings/store'
import type { AppSettings } from '@/features/settings/types'

const settingsStore = useSettingsStore()
const { t } = useI18n()
const { toast } = useToast()

type Step = 1 | 2 | 3
type Patch = Partial<AppSettings['extensions']['infinityNikki']>

const step = ref<Step>(1)
const wantsMetadata = ref(false)
const wantsHardlinks = ref(false)
const isSubmitting = ref(false)

async function persist(patch: Patch) {
  await settingsStore.updateSettings({
    extensions: {
      infinityNikki: {
        ...settingsStore.appSettings.extensions.infinityNikki,
        galleryGuideSeen: true,
        ...patch,
      },
    },
  })
}

// 步骤 1：记录选择，前进到步骤 2
function selectMetadata(enable: boolean) {
  wantsMetadata.value = enable
  step.value = 2
}

// 步骤 2：记录选择，前进到步骤 3
function selectHardlinks(enable: boolean) {
  wantsHardlinks.value = enable
  step.value = 3
}

// 步骤 3：保存前两步的选择结果
async function applySelections() {
  if (isSubmitting.value) return
  isSubmitting.value = true
  try {
    await persist({
      allowOnlinePhotoMetadataExtract: wantsMetadata.value,
      manageMediaHardlinks: wantsHardlinks.value,
    })

    if (wantsMetadata.value && wantsHardlinks.value) {
      toast.success(t('gallery.guide.infinityNikki.recommendedTaskStartedTitle'), {
        description: t('gallery.guide.infinityNikki.recommendedTaskStartedDescription'),
      })
    } else if (wantsMetadata.value) {
      toast.success(t('gallery.guide.infinityNikki.metadataTaskStartedTitle'), {
        description: t('gallery.guide.infinityNikki.metadataTaskStartedDescription'),
      })
    } else if (wantsHardlinks.value) {
      toast.success(t('gallery.guide.infinityNikki.hardlinksTaskStartedTitle'), {
        description: t('gallery.guide.infinityNikki.hardlinksTaskStartedDescription'),
      })
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.guide.infinityNikki.actionFailedTitle'), { description: message })
  } finally {
    isSubmitting.value = false
  }
}

// Footer 统一路由到当前步骤的处理函数
function handleEnable() {
  if (step.value === 1) {
    selectMetadata(true)
  } else if (step.value === 2) {
    selectHardlinks(true)
  } else {
    void applySelections()
  }
}

function handleSkip() {
  if (step.value === 1) {
    selectMetadata(false)
  } else {
    selectHardlinks(false)
  }
}

function handlePrevious() {
  if (isSubmitting.value || step.value === 1) return
  step.value = step.value === 3 ? 2 : 1
}
</script>

<template>
  <div class="relative flex h-full w-full items-center justify-center overflow-hidden p-6">
    <!-- Background Watermark: Zongzi Momo (Outer Canvas Accent) -->
    <div
      class="pointer-events-none absolute top-4 right-20 bottom-6 z-0 w-auto max-w-full text-foreground/5 opacity-20 select-none dark:text-white/4 [&_path]:fill-current"
      aria-hidden="true"
    >
      <div
        class="flex h-full w-full items-center justify-end [&_svg]:h-full [&_svg]:w-auto [&_svg]:max-w-none [&_svg]:shrink-0"
        v-html="zongziMomoSvg"
      ></div>
    </div>

    <!-- 卡片主体 (叠在水印上方) -->
    <div
      class="relative z-10 flex w-full max-w-2xl flex-col rounded-md bg-background/80 dark:bg-background/85"
    >
      <!-- Header：说明来源 + 步骤进度 -->
      <div class="flex shrink-0 items-center justify-between px-6 pt-5 pb-3">
        <div class="flex items-center">
          <div>
            <span class="text-[0.65rem] font-medium tracking-[0.2em] text-primary uppercase">
              INFINITY NIKKI • GALLERY SETUP
            </span>
            <p class="mt-0.5 text-xs text-muted-foreground">
              {{ t('gallery.guide.infinityNikki.header.subtitle') }}
            </p>
          </div>
        </div>

        <!-- 步骤指示器：当前步骤圆点加宽 -->
        <div class="flex items-center gap-1.5">
          <div
            class="h-1.5 rounded-full transition-all duration-300"
            :class="step === 1 ? 'w-6 bg-primary' : 'w-2 bg-primary/30'"
          />
          <div
            class="h-1.5 rounded-full transition-all duration-300"
            :class="step === 2 ? 'w-6 bg-primary' : 'w-2 bg-muted-foreground/20'"
          />
          <div
            class="h-1.5 rounded-full transition-all duration-300"
            :class="step === 3 ? 'w-6 bg-primary' : 'w-2 bg-muted-foreground/20'"
          />
        </div>
      </div>

      <!-- Body：横向双栏 —— 左侧图标，右侧文字内容 -->
      <div class="flex min-h-[220px] flex-1 items-center gap-6 px-6 py-4">
        <!-- 左栏：精简图标 -->
        <div class="flex w-16 shrink-0 items-center justify-center">
          <Transition name="guide-icon" mode="out-in">
            <div
              :key="step"
              class="surface-top flex size-14 items-center justify-center rounded-md border border-primary/20 text-primary"
            >
              <ScanText v-if="step === 1" class="size-7" :stroke-width="1.5" />
              <FolderSymlink v-else-if="step === 2" class="size-7" :stroke-width="1.5" />
              <Sparkles v-else class="size-7" :stroke-width="1.5" />
            </div>
          </Transition>
        </div>

        <!-- 右栏：步骤内容，切换时左右滑动 -->
        <div class="flex-1 overflow-hidden">
          <Transition name="guide-step" mode="out-in">
            <!-- 步骤 1：照片元数据解析 -->
            <div v-if="step === 1" key="step-1" class="space-y-3">
              <h2 class="text-2xl font-bold tracking-tight text-foreground">
                {{ t('gallery.guide.infinityNikki.step1.title') }}
              </h2>
              <p class="text-sm leading-relaxed text-muted-foreground">
                {{ t('gallery.guide.infinityNikki.metadataDescription') }}
              </p>
              <div class="space-y-1 pt-1">
                <p class="text-xs leading-relaxed text-muted-foreground">
                  <span>
                    {{ t('gallery.guide.infinityNikki.credit') }}
                    <a
                      href="https://NUAN5.PRO"
                      target="_blank"
                      rel="noopener noreferrer"
                      class="font-medium text-green-500 transition-colors hover:text-green-600 dark:text-green-400 dark:hover:text-green-300"
                    >
                      {{ t('gallery.guide.infinityNikki.creditLink') }}
                    </a>
                    {{ t('gallery.guide.infinityNikki.creditPowered') }}
                  </span>
                </p>
                <p class="text-xs leading-relaxed text-muted-foreground/60">
                  {{ t('gallery.guide.infinityNikki.step1.mapHint') }}
                </p>
              </div>
            </div>

            <!-- 步骤 2：硬链接优化 -->
            <div v-else-if="step === 2" key="step-2" class="space-y-3">
              <h2 class="text-2xl font-bold tracking-tight text-foreground">
                {{ t('gallery.guide.infinityNikki.step2.title') }}
              </h2>
              <p class="text-sm leading-relaxed text-muted-foreground">
                {{ t('gallery.guide.infinityNikki.hardlinksDescription') }}
              </p>
              <p
                class="rounded-md border border-border/40 bg-background/50 px-3.5 py-2.5 text-xs leading-relaxed whitespace-pre-wrap text-muted-foreground"
              >
                {{ t('gallery.guide.infinityNikki.hardlinksDetailsContent') }}
              </p>
            </div>

            <!-- 步骤 3：执行前提醒 -->
            <div v-else key="step-3" class="space-y-3">
              <h2 class="text-2xl font-bold tracking-tight text-foreground">
                {{ t('gallery.guide.infinityNikki.step3.title') }}
              </h2>
              <p class="text-sm leading-relaxed text-muted-foreground">
                {{ t('gallery.guide.infinityNikki.step3.description') }}
              </p>
              <p
                class="rounded-md border border-border/40 bg-background/50 px-3.5 py-2.5 text-xs leading-relaxed text-muted-foreground"
              >
                {{ t('gallery.guide.infinityNikki.step3.timeCostNotice') }}
              </p>
            </div>
          </Transition>
        </div>
      </div>

      <!-- Footer：操作按钮 -->
      <div class="flex shrink-0 items-center justify-between gap-3 px-6 py-4">
        <div>
          <Button
            v-if="step !== 1"
            variant="ghost"
            :disabled="isSubmitting"
            @click="handlePrevious"
          >
            {{ t('onboarding.actions.previous') }}
          </Button>
        </div>

        <div v-if="step !== 3" class="flex items-center gap-3">
          <Button variant="ghost" :disabled="isSubmitting" @click="handleSkip">
            {{ t('gallery.guide.infinityNikki.actions.skip') }}
          </Button>
          <Button class="gap-1.5" :disabled="isSubmitting" @click="handleEnable">
            {{ t('gallery.guide.infinityNikki.actions.enable') }}
            <ChevronRight class="size-4" />
          </Button>
        </div>
        <div v-else class="flex items-center gap-3">
          <Button class="gap-1.5" :disabled="isSubmitting" @click="handleEnable">
            {{ t('gallery.guide.infinityNikki.actions.confirmAndApply') }}
            <ChevronRight class="size-4" />
          </Button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
/* 右栏内容：向左滑出，从右滑入 */
.guide-step-enter-active,
.guide-step-leave-active {
  transition:
    opacity 0.18s ease,
    transform 0.18s ease;
}

.guide-step-enter-from {
  opacity: 0;
  transform: translateX(14px);
}

.guide-step-leave-to {
  opacity: 0;
  transform: translateX(-14px);
}

/* 左栏图标：简单淡入淡出 */
.guide-icon-enter-active,
.guide-icon-leave-active {
  transition: opacity 0.15s ease;
}

.guide-icon-enter-from,
.guide-icon-leave-to {
  opacity: 0;
}
</style>
