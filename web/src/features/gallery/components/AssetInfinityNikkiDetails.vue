<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Separator } from '@/components/ui/separator'
import { readClipboardText } from '@/core/clipboard'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { updateInfinityNikkiDyeCode } from '../api'
import type { InfinityNikkiPhotoParams } from '../types'

const props = defineProps<{
  assetId: number
  params: InfinityNikkiPhotoParams
}>()

const emit = defineEmits<{
  updated: [params: InfinityNikkiPhotoParams]
}>()

const { t } = useI18n()
const { toast } = useToast()

const dyeCodeDraft = ref('')
const isSavingDyeCode = ref(false)
const hasDyeCodeDraft = computed(() => dyeCodeDraft.value.trim().length > 0)

watch(
  () => props.params,
  (p) => {
    dyeCodeDraft.value = p.dyeCode ?? ''
  },
  { deep: true, immediate: true }
)

function padTwoDigits(value: number): string {
  return String(value).padStart(2, '0')
}

function formatGameTime(params: InfinityNikkiPhotoParams): string | null {
  if (params.timeHour === undefined || params.timeMin === undefined) return null
  return `${padTwoDigits(params.timeHour)}:${padTwoDigits(params.timeMin)}`
}

function formatNumber(value: number | undefined, digits = 2): string | null {
  if (value === undefined) return null
  return Number(value)
    .toFixed(digits)
    .replace(/\.?0+$/, '')
}

function copyWithExecCommand(text: string): boolean {
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

async function copyTextWithFallback(text: string): Promise<boolean> {
  if (typeof navigator !== 'undefined' && navigator.clipboard?.writeText) {
    try {
      await navigator.clipboard.writeText(text)
      return true
    } catch {
      // fall through to execCommand
    }
  }
  return copyWithExecCommand(text)
}

function resetDyeCodeDraft() {
  dyeCodeDraft.value = props.params.dyeCode ?? ''
}

async function handleDyeCodeCommit() {
  if (isSavingDyeCode.value) {
    return
  }

  const normalizedDyeCode = dyeCodeDraft.value.trim()
  const currentDyeCode = (props.params.dyeCode ?? '').trim()
  dyeCodeDraft.value = normalizedDyeCode

  if (normalizedDyeCode === currentDyeCode) {
    return
  }

  isSavingDyeCode.value = true

  try {
    await updateInfinityNikkiDyeCode({
      assetId: props.assetId,
      dyeCode: normalizedDyeCode || undefined,
    })

    emit('updated', {
      ...props.params,
      dyeCode: normalizedDyeCode || undefined,
    })
  } catch (error) {
    resetDyeCodeDraft()
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.details.infinityNikki.updateDyeCodeFailed'), {
      description: message,
    })
  } finally {
    isSavingDyeCode.value = false
  }
}

async function handleCopyCameraParams(text: string) {
  const success = await copyTextWithFallback(text)
  if (success) {
    toast.success(t('gallery.details.infinityNikki.copyCameraParamsSuccess'))
    return
  }
  toast.error(t('gallery.details.infinityNikki.copyCameraParamsFailed'))
}

async function handleDyeCodeAction() {
  if (hasDyeCodeDraft.value) {
    const success = await copyTextWithFallback(dyeCodeDraft.value.trim())
    if (success) {
      toast.success(t('gallery.details.infinityNikki.copyDyeCodeSuccess'))
      return
    }
    toast.error(t('gallery.details.infinityNikki.copyDyeCodeFailed'))
    return
  }

  let normalizedClipboardText = ''
  try {
    normalizedClipboardText = (await readClipboardText())?.trim() ?? ''
  } catch {
    normalizedClipboardText = ''
  }

  if (!normalizedClipboardText) {
    toast.error(t('gallery.details.infinityNikki.pasteDyeCodeFailed'))
    return
  }

  dyeCodeDraft.value = normalizedClipboardText
}

function formatPercentage(value: number | undefined): string | null {
  if (value === undefined) return null
  return `${formatNumber(value * 100, 1) ?? '0'}%`
}

function formatFocalLength(value: number | undefined): string | null {
  const formatted = formatNumber(value)
  return formatted ? `${formatted} mm` : null
}

function formatApertureSection(value: number | undefined): string | null {
  if (value === undefined) return null
  return `${value} ${t('gallery.details.infinityNikki.apertureSectionUnit')}`
}

function formatMetadataText(value: string | undefined): string | null {
  if (!value) return null
  const trimmed = value.trim()
  if (!trimmed || trimmed.toLowerCase() === 'none') return null
  return trimmed
}

function formatPoseId(value: number | undefined): string | null {
  if (value === undefined || value === 0) return null
  return String(value)
}
</script>

<template>
  <Separator />

  <div class="space-y-3">
    <h4 class="text-sm font-medium">{{ t('gallery.details.infinityNikki.title') }}</h4>
    <div class="space-y-2 text-xs">
      <div class="flex items-center justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.infinityNikki.dyeCode') }}</span>
        <div class="flex max-w-42 min-w-0 flex-1 items-center gap-2">
          <Input
            v-model="dyeCodeDraft"
            :disabled="isSavingDyeCode"
            class="h-6 min-w-0 flex-1 px-2 text-xs md:text-xs"
            @blur="handleDyeCodeCommit"
            @keydown.enter.prevent="handleDyeCodeCommit"
            @keydown.esc.prevent="resetDyeCodeDraft"
          />
          <Button
            variant="outline"
            size="sm"
            class="h-6 px-2 text-xs"
            :disabled="isSavingDyeCode"
            @click="handleDyeCodeAction"
          >
            {{
              hasDyeCodeDraft
                ? t('gallery.details.infinityNikki.copyDyeCode')
                : t('gallery.details.infinityNikki.pasteDyeCode')
            }}
          </Button>
        </div>
      </div>

      <div v-if="formatGameTime(params)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.infinityNikki.gameTime') }}</span>
        <span>{{ formatGameTime(params) }}</span>
      </div>

      <div v-if="params.cameraParams" class="flex items-center justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.cameraParams')
        }}</span>
        <Button
          variant="outline"
          size="sm"
          class="h-6 px-2 text-xs"
          @click="handleCopyCameraParams(params.cameraParams!)"
        >
          {{ t('gallery.details.infinityNikki.copyCameraParams') }}
        </Button>
      </div>

      <div v-if="formatFocalLength(params.cameraFocalLength)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.cameraFocalLength')
        }}</span>
        <span>{{ formatFocalLength(params.cameraFocalLength) }}</span>
      </div>
      <div v-if="formatApertureSection(params.apertureSection)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.apertureSection')
        }}</span>
        <span>{{ formatApertureSection(params.apertureSection) }}</span>
      </div>
      <div v-if="formatMetadataText(params.filterId)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.infinityNikki.filterId') }}</span>
        <span
          class="max-w-32 truncate font-mono"
          :title="formatMetadataText(params.filterId) ?? undefined"
        >
          {{ formatMetadataText(params.filterId) }}
        </span>
      </div>
      <div v-if="formatPercentage(params.filterStrength)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.filterStrength')
        }}</span>
        <span>{{ formatPercentage(params.filterStrength) }}</span>
      </div>
      <div v-if="formatPercentage(params.vignetteIntensity)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.vignetteIntensity')
        }}</span>
        <span>{{ formatPercentage(params.vignetteIntensity) }}</span>
      </div>
      <div v-if="formatMetadataText(params.lightId)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.infinityNikki.lightId') }}</span>
        <span
          class="max-w-32 truncate font-mono"
          :title="formatMetadataText(params.lightId) ?? undefined"
        >
          {{ formatMetadataText(params.lightId) }}
        </span>
      </div>
      <div v-if="formatPercentage(params.lightStrength)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.lightStrength')
        }}</span>
        <span>{{ formatPercentage(params.lightStrength) }}</span>
      </div>
      <div v-if="params.nikkiHidden !== undefined" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{
          t('gallery.details.infinityNikki.nikkiHidden')
        }}</span>
        <span>{{ params.nikkiHidden ? t('common.yes') : t('common.no') }}</span>
      </div>
      <div v-if="formatPoseId(params.poseId)" class="flex justify-between gap-2">
        <span class="text-muted-foreground">{{ t('gallery.details.infinityNikki.poseId') }}</span>
        <span>{{ formatPoseId(params.poseId) }}</span>
      </div>
    </div>
  </div>
</template>
