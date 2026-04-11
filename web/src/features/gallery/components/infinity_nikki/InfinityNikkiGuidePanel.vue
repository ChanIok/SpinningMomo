<script setup lang="ts">
import { ref } from 'vue'
import { Button } from '@/components/ui/button'
import { ScanText, FolderSymlink, Sparkles } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useSettingsStore } from '@/features/settings/store'
import type { AppSettings } from '@/features/settings/types'

const settingsStore = useSettingsStore()
const { t } = useI18n()
const { toast } = useToast()

type Step = 1 | 2
type Patch = Partial<AppSettings['extensions']['infinityNikki']>

const step = ref<Step>(1)
const wantsMetadata = ref(false)
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

// 步骤 2：保存两步的选择结果
async function selectHardlinks(enable: boolean) {
  if (isSubmitting.value) return
  isSubmitting.value = true
  try {
    await persist({
      allowOnlinePhotoMetadataExtract: wantsMetadata.value,
      manageScreenshotHardlinks: enable,
    })

    if (wantsMetadata.value && enable) {
      toast.success(t('gallery.guide.infinityNikki.recommendedTaskStartedTitle'), {
        description: t('gallery.guide.infinityNikki.recommendedTaskStartedDescription'),
      })
    } else if (wantsMetadata.value) {
      toast.success(t('gallery.guide.infinityNikki.metadataTaskStartedTitle'), {
        description: t('gallery.guide.infinityNikki.metadataTaskStartedDescription'),
      })
    } else if (enable) {
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
  } else {
    void selectHardlinks(true)
  }
}

function handleSkip() {
  if (step.value === 1) {
    selectMetadata(false)
  } else {
    void selectHardlinks(false)
  }
}

function handlePrevious() {
  if (isSubmitting.value || step.value === 1) return
  step.value = 1
}
</script>

<template>
  <div class="flex h-full w-full items-center justify-center overflow-y-auto p-6">
    <!-- 卡片主体 -->
    <div class="flex w-full max-w-2xl flex-col rounded-md border bg-card shadow-sm">
      <!-- Header：说明来源 + 步骤进度 -->
      <div class="flex shrink-0 items-center justify-between px-6 py-4">
        <div class="flex items-center gap-3">
          <div
            class="flex size-8 shrink-0 items-center justify-center rounded-lg bg-primary/10 text-primary"
          >
            <Sparkles class="size-4" />
          </div>
          <div>
            <p class="text-sm leading-tight font-semibold text-foreground">
              {{ t('gallery.guide.infinityNikki.header.title') }}
            </p>
            <p class="text-xs text-muted-foreground">
              {{ t('gallery.guide.infinityNikki.header.subtitle') }}
            </p>
          </div>
        </div>

        <!-- 步骤指示器：当前步骤圆点加宽 -->
        <div class="flex items-center gap-1.5">
          <div
            class="h-1.5 rounded-full transition-all duration-300"
            :class="step === 1 ? 'w-6 bg-primary' : 'w-2.5 bg-primary/30'"
          />
          <div
            class="h-1.5 rounded-full transition-all duration-300"
            :class="step === 2 ? 'w-6 bg-primary' : 'w-2.5 bg-muted-foreground/20'"
          />
        </div>
      </div>

      <!-- Body：横向双栏 —— 左侧大图标，右侧文字内容 -->
      <div class="flex min-h-[248px] flex-1 items-center gap-8 px-6 py-6">
        <!-- 左栏：大图标，随步骤切换淡入淡出 -->
        <div class="flex w-24 shrink-0 items-center justify-center">
          <Transition name="guide-icon" mode="out-in">
            <div
              :key="step"
              class="flex size-20 items-center justify-center rounded-2xl bg-primary/10 text-primary"
            >
              <ScanText v-if="step === 1" class="size-10" />
              <FolderSymlink v-else class="size-10" />
            </div>
          </Transition>
        </div>

        <!-- 右栏：步骤内容，切换时左右滑动 -->
        <div class="flex-1 overflow-hidden">
          <Transition name="guide-step" mode="out-in">
            <!-- 步骤 1：照片元数据解析 -->
            <div v-if="step === 1" key="step-1" class="space-y-2">
              <h2 class="text-base font-semibold text-foreground">
                {{ t('gallery.guide.infinityNikki.step1.title') }}
              </h2>
              <p class="text-sm leading-relaxed text-muted-foreground">
                {{ t('gallery.guide.infinityNikki.metadataDescription') }}
              </p>
            </div>

            <!-- 步骤 2：硬链接优化 -->
            <div v-else key="step-2" class="space-y-3">
              <h2 class="text-base font-semibold text-foreground">
                {{ t('gallery.guide.infinityNikki.step2.title') }}
              </h2>
              <p class="text-sm leading-relaxed text-muted-foreground">
                {{ t('gallery.guide.infinityNikki.hardlinksDescription') }}
              </p>
              <!-- 细节说明直接展示，不再折叠 -->
              <p
                class="rounded-lg bg-muted/50 px-3 py-2.5 text-xs leading-relaxed whitespace-pre-wrap text-muted-foreground"
              >
                {{ t('gallery.guide.infinityNikki.hardlinksDetailsContent') }}
              </p>
            </div>
          </Transition>
        </div>
      </div>

      <!-- Footer：操作按钮 -->
      <div class="flex shrink-0 items-center justify-between gap-3 border-t px-6 py-4">
        <div>
          <Button
            v-if="step === 2"
            variant="ghost"
            :disabled="isSubmitting"
            @click="handlePrevious"
          >
            {{ t('onboarding.actions.previous') }}
          </Button>
        </div>

        <div class="flex items-center gap-3">
          <Button variant="ghost" :disabled="isSubmitting" @click="handleSkip">
            {{ t('gallery.guide.infinityNikki.actions.skip') }}
          </Button>
          <Button :disabled="isSubmitting" @click="handleEnable">
            {{ t('gallery.guide.infinityNikki.actions.enable') }}
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
