<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Check, ChevronDown } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { Separator } from '@/components/ui/separator'
import { readClipboardText } from '@/core/clipboard'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { setInfinityNikkiUserRecord } from '../../api'
import type {
  InfinityNikkiDetails,
  InfinityNikkiExtractedParams,
  InfinityNikkiUserRecordCodeType,
} from '../../types'

const props = defineProps<{
  assetId: number
  details: InfinityNikkiDetails
}>()

const emit = defineEmits<{
  updated: [details: InfinityNikkiDetails]
}>()

const { t } = useI18n()
const { toast } = useToast()

const codeTypeDraft = ref<InfinityNikkiUserRecordCodeType>('dye')
const codeValueDraft = ref('')
const isSavingUserRecord = ref(false)
const isCodeTypePopoverOpen = ref(false)

const extracted = computed(() => props.details.extracted)
const currentUserRecord = computed(() => props.details.userRecord)
const hasCodeValueDraft = computed(() => codeValueDraft.value.trim().length > 0)
const currentCodeTypeLabel = computed(() => getCodeTypeLabel(codeTypeDraft.value))

function syncDraftFromProps() {
  codeTypeDraft.value = currentUserRecord.value?.codeType ?? 'dye'
  codeValueDraft.value = currentUserRecord.value?.codeValue ?? ''
}

watch(
  () => props.details,
  () => {
    syncDraftFromProps()
  },
  { deep: true, immediate: true }
)

function getCodeTypeLabel(codeType: InfinityNikkiUserRecordCodeType): string {
  return codeType === 'home_building'
    ? t('gallery.details.infinityNikki.codeType.homeBuilding')
    : t('gallery.details.infinityNikki.codeType.dye')
}

function padTwoDigits(value: number): string {
  return String(value).padStart(2, '0')
}

function formatGameTime(params: InfinityNikkiExtractedParams | undefined): string | null {
  if (!params || params.timeHour === undefined || params.timeMin === undefined) return null
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

function resetUserRecordDraft() {
  syncDraftFromProps()
}

async function handleUserRecordCommit() {
  if (isSavingUserRecord.value) {
    return
  }

  const normalizedCodeValue = codeValueDraft.value.trim()
  const nextCodeType = codeTypeDraft.value
  const currentCodeType = currentUserRecord.value?.codeType ?? 'dye'
  const currentCodeValue = (currentUserRecord.value?.codeValue ?? '').trim()
  codeValueDraft.value = normalizedCodeValue

  if (!currentUserRecord.value && !normalizedCodeValue) {
    return
  }

  if (normalizedCodeValue === currentCodeValue && nextCodeType === currentCodeType) {
    return
  }

  isSavingUserRecord.value = true

  try {
    await setInfinityNikkiUserRecord({
      assetId: props.assetId,
      codeType: nextCodeType,
      codeValue: normalizedCodeValue || undefined,
    })

    emit('updated', {
      extracted: props.details.extracted,
      userRecord: normalizedCodeValue
        ? {
            codeType: nextCodeType,
            codeValue: normalizedCodeValue,
          }
        : undefined,
    })
  } catch (error) {
    resetUserRecordDraft()
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.details.infinityNikki.saveUserRecordFailed'), {
      description: message,
    })
  } finally {
    isSavingUserRecord.value = false
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

async function handleCodeValueAction() {
  if (hasCodeValueDraft.value) {
    const success = await copyTextWithFallback(codeValueDraft.value.trim())
    if (success) {
      toast.success(t('gallery.details.infinityNikki.copyCodeValueSuccess'))
      return
    }
    toast.error(t('gallery.details.infinityNikki.copyCodeValueFailed'))
    return
  }

  let normalizedClipboardText = ''
  try {
    normalizedClipboardText = (await readClipboardText())?.trim() ?? ''
  } catch {
    normalizedClipboardText = ''
  }

  if (!normalizedClipboardText) {
    toast.error(t('gallery.details.infinityNikki.pasteCodeValueFailed'))
    return
  }

  codeValueDraft.value = normalizedClipboardText
}

async function handleSelectCodeType(nextCodeType: InfinityNikkiUserRecordCodeType) {
  isCodeTypePopoverOpen.value = false
  if (codeTypeDraft.value === nextCodeType) {
    return
  }

  codeTypeDraft.value = nextCodeType
  if (hasCodeValueDraft.value || currentUserRecord.value) {
    await handleUserRecordCommit()
  }
}

function formatPercentage(value: number | undefined): string | null {
  if (value === undefined) return null
  return `${formatNumber(value, 1) ?? '0'}%`
}

function formatFocalLength(value: number | undefined): string | null {
  const formatted = formatNumber(value)
  return formatted ? `${formatted} mm` : null
}

function formatApertureValue(value: number | undefined): string | null {
  const formatted = formatNumber(value, 1)
  return formatted ? `f / ${formatted}` : null
}

function formatSignedNumber(value: number | undefined, digits = 2): string | null {
  if (value === undefined) return null
  const formatted = formatNumber(value, digits)
  if (!formatted) return null
  return value > 0 ? `+${formatted}` : formatted
}

function formatMetadataId(value: number | undefined): string | null {
  if (value === undefined || value === null) return null
  return String(value)
}

function formatPoseId(value: number | undefined): string | null {
  if (value === undefined || value === null || value === 0) return null
  return String(value)
}

function formatLocation(params: InfinityNikkiExtractedParams | undefined): string | null {
  if (!params) return null
  const x = formatNumber(params.nikkiLocX, 2)
  const y = formatNumber(params.nikkiLocY, 2)
  if (!x || !y) return null

  const z = formatNumber(params.nikkiLocZ, 2)
  return z ? `(${x}, ${y}, ${z})` : `(${x}, ${y})`
}
</script>

<template>
  <Separator />

  <div class="space-y-3">
    <h4 class="text-sm font-medium">{{ t('gallery.details.infinityNikki.title') }}</h4>

    <div class="space-y-2 text-xs">
      <div class="flex items-center justify-between gap-2">
        <div class="flex min-w-0 items-center gap-1 text-muted-foreground">
          <span>{{ currentCodeTypeLabel }}</span>
          <Popover v-model:open="isCodeTypePopoverOpen">
            <PopoverTrigger as-child>
              <Button variant="ghost" size="icon" class="h-5 w-5 text-muted-foreground">
                <ChevronDown class="h-3 w-3" />
              </Button>
            </PopoverTrigger>
            <PopoverContent align="start" class="w-36 p-1">
              <button
                type="button"
                class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-xs transition-colors hover:bg-accent"
                @click="void handleSelectCodeType('dye')"
              >
                <span>{{ t('gallery.details.infinityNikki.codeType.dye') }}</span>
                <Check v-if="codeTypeDraft === 'dye'" class="h-3.5 w-3.5" />
              </button>
              <button
                type="button"
                class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-xs transition-colors hover:bg-accent"
                @click="void handleSelectCodeType('home_building')"
              >
                <span>{{ t('gallery.details.infinityNikki.codeType.homeBuilding') }}</span>
                <Check v-if="codeTypeDraft === 'home_building'" class="h-3.5 w-3.5" />
              </button>
            </PopoverContent>
          </Popover>
        </div>

        <div class="flex max-w-54 min-w-0 flex-1 items-center gap-2">
          <Input
            v-model="codeValueDraft"
            :disabled="isSavingUserRecord"
            class="h-6 min-w-0 flex-1 px-2 text-xs md:text-xs"
            @blur="handleUserRecordCommit"
            @keydown.enter.prevent="handleUserRecordCommit"
            @keydown.esc.prevent="resetUserRecordDraft"
          />
          <Button
            variant="outline"
            size="sm"
            class="h-6 px-2 text-xs"
            :disabled="isSavingUserRecord"
            @click="handleCodeValueAction"
          >
            {{
              hasCodeValueDraft
                ? t('gallery.details.infinityNikki.copyCodeValue')
                : t('gallery.details.infinityNikki.pasteCodeValue')
            }}
          </Button>
        </div>
      </div>

      <template v-if="extracted">
        <div v-if="formatGameTime(extracted)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.gameTime')
          }}</span>
          <span>{{ formatGameTime(extracted) }}</span>
        </div>

        <div v-if="extracted.cameraParams" class="flex items-center justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.cameraParams')
          }}</span>
          <Button
            variant="outline"
            size="sm"
            class="h-6 px-2 text-xs"
            @click="handleCopyCameraParams(extracted.cameraParams)"
          >
            {{ t('gallery.details.infinityNikki.copyCameraParams') }}
          </Button>
        </div>

        <div
          v-if="formatFocalLength(extracted.cameraFocalLength)"
          class="flex justify-between gap-2"
        >
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.cameraFocalLength')
          }}</span>
          <span>{{ formatFocalLength(extracted.cameraFocalLength) }}</span>
        </div>
        <div v-if="formatApertureValue(extracted.apertureValue)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.apertureValue')
          }}</span>
          <span>{{ formatApertureValue(extracted.apertureValue) }}</span>
        </div>
        <div v-if="extracted.vertical !== undefined" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.vertical')
          }}</span>
          <span>{{ extracted.vertical ? t('common.yes') : t('common.no') }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.rotation, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.rotation')
          }}</span>
          <span>{{ formatSignedNumber(extracted.rotation, 2) }}</span>
        </div>
        <div v-if="formatMetadataId(extracted.filterId)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.filterId')
          }}</span>
          <span
            class="max-w-32 truncate"
            :title="formatMetadataId(extracted.filterId) ?? undefined"
          >
            {{ formatMetadataId(extracted.filterId) }}
          </span>
        </div>
        <div v-if="formatPercentage(extracted.filterStrength)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.filterStrength')
          }}</span>
          <span>{{ formatPercentage(extracted.filterStrength) }}</span>
        </div>
        <div
          v-if="formatPercentage(extracted.vignetteIntensity)"
          class="flex justify-between gap-2"
        >
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.vignetteIntensity')
          }}</span>
          <span>{{ formatPercentage(extracted.vignetteIntensity) }}</span>
        </div>
        <div v-if="formatPercentage(extracted.bloomIntensity)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.bloomIntensity')
          }}</span>
          <span>{{ formatPercentage(extracted.bloomIntensity) }}</span>
        </div>
        <div
          v-if="formatSignedNumber(extracted.bloomThreshold, 2)"
          class="flex justify-between gap-2"
        >
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.bloomThreshold')
          }}</span>
          <span>{{ formatSignedNumber(extracted.bloomThreshold, 2) }}</span>
        </div>
        <div v-if="formatPercentage(extracted.brightness)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.brightness')
          }}</span>
          <span>{{ formatPercentage(extracted.brightness) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.exposure, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.exposure')
          }}</span>
          <span>{{ formatSignedNumber(extracted.exposure, 2) }}</span>
        </div>
        <div v-if="formatPercentage(extracted.contrast)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.contrast')
          }}</span>
          <span>{{ formatPercentage(extracted.contrast) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.saturation, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.saturation')
          }}</span>
          <span>{{ formatSignedNumber(extracted.saturation, 2) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.vibrance, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.vibrance')
          }}</span>
          <span>{{ formatSignedNumber(extracted.vibrance, 2) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.highlights, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.highlights')
          }}</span>
          <span>{{ formatSignedNumber(extracted.highlights, 2) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.shadow, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.shadow')
          }}</span>
          <span>{{ formatSignedNumber(extracted.shadow, 2) }}</span>
        </div>
        <div v-if="formatMetadataId(extracted.lightId)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.lightId')
          }}</span>
          <span class="max-w-32 truncate" :title="formatMetadataId(extracted.lightId) ?? undefined">
            {{ formatMetadataId(extracted.lightId) }}
          </span>
        </div>
        <div v-if="formatPercentage(extracted.lightStrength)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.lightStrength')
          }}</span>
          <span>{{ formatPercentage(extracted.lightStrength) }}</span>
        </div>
        <div v-if="formatLocation(extracted)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.nikkiLocation')
          }}</span>
          <span class="truncate" :title="formatLocation(extracted) ?? undefined">{{
            formatLocation(extracted)
          }}</span>
        </div>
        <div v-if="extracted.nikkiHidden !== undefined" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.nikkiHidden')
          }}</span>
          <span>{{ extracted.nikkiHidden ? t('common.yes') : t('common.no') }}</span>
        </div>
        <div v-if="formatPoseId(extracted.poseId)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.poseId')
          }}</span>
          <span>{{ formatPoseId(extracted.poseId) }}</span>
        </div>
      </template>
    </div>
  </div>
</template>
