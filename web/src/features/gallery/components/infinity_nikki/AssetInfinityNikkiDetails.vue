<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { Check, ChevronDown } from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Popover, PopoverContent, PopoverTrigger } from '@/components/ui/popover'
import { Separator } from '@/components/ui/separator'
import { readClipboardText } from '@/core/clipboard'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useMapStore } from '@/features/map/store'
import { transformGameToMapCoordinates } from '@/features/map/domain/coordinates'
import { toOfficialWorldIdWithDefaultVersion } from '@/features/map/domain/officialWorldId'
import {
  fillInfinityNikkiSameOutfitDyeCode,
  getInfinityNikkiMetadataNames,
  previewInfinityNikkiSameOutfitDyeCodeFill,
  setInfinityNikkiUserRecord,
  setInfinityNikkiWorldRecord,
} from '@/extensions/infinity_nikki/api'
import {
  getInfinityNikkiWorldName,
  INFINITY_NIKKI_WORLD_OPTIONS,
  normalizeInfinityNikkiWorldId,
} from '@/extensions/infinity_nikki/worlds'
import type {
  InfinityNikkiDetails,
  InfinityNikkiExtractedParams,
  InfinityNikkiMetadataNames,
  InfinityNikkiSameOutfitDyeCodeFillPreview,
  InfinityNikkiUserRecordCodeType,
} from '@/extensions/infinity_nikki/types'

const props = defineProps<{
  assetId: number
  details: InfinityNikkiDetails
}>()

const emit = defineEmits<{
  updated: [details: InfinityNikkiDetails]
}>()

const { t, locale } = useI18n()
const { toast } = useToast()
const router = useRouter()
const mapStore = useMapStore()

const codeTypeDraft = ref<InfinityNikkiUserRecordCodeType>('dye')
const codeValueDraft = ref('')
const isSavingUserRecord = ref(false)
const isCodeTypePopoverOpen = ref(false)
const isSavingWorldRecord = ref(false)
const isWorldPopoverOpen = ref(false)
const isFillingSameOutfitDyeCode = ref(false)
const sameOutfitDyeFillPreview = ref<InfinityNikkiSameOutfitDyeCodeFillPreview | null>(null)
const sameOutfitDyeFillPreviewCodeValue = ref('')

const extracted = computed(() => props.details.extracted)
const currentUserRecord = computed(() => props.details.userRecord)
const currentMapArea = computed(() => props.details.mapArea)
const hasCodeValueDraft = computed(() => codeValueDraft.value.trim().length > 0)
const currentCodeTypeLabel = computed(() => getCodeTypeLabel(codeTypeDraft.value))
const canShowSameOutfitDyeFillPrompt = computed(() => {
  const preview = sameOutfitDyeFillPreview.value
  return (
    codeTypeDraft.value === 'dye' &&
    preview?.sourceHasOutfitDyeState === true &&
    preview.matchedCount > 0 &&
    sameOutfitDyeFillPreviewCodeValue.value.length > 0 &&
    sameOutfitDyeFillPreviewCodeValue.value === codeValueDraft.value.trim()
  )
})
const currentWorldId = computed(() => currentMapArea.value?.worldId)
const currentWorldLabel = computed(() =>
  currentWorldId.value ? getInfinityNikkiWorldName(currentWorldId.value, locale.value) : null
)
const currentMapLocationTarget = computed(() => {
  const params = extracted.value
  const worldId = toOfficialWorldIdWithDefaultVersion(currentMapArea.value?.worldId)
  if (!params || !worldId || params.nikkiLocX === undefined || params.nikkiLocY === undefined) {
    return null
  }

  const { lat, lng } = transformGameToMapCoordinates(
    {
      nikkiLocX: params.nikkiLocX,
      nikkiLocY: params.nikkiLocY,
    },
    worldId
  )
  if (!Number.isFinite(lat) || !Number.isFinite(lng)) {
    return null
  }

  return {
    lat,
    lng,
    worldId,
  }
})
// 当前面板实际使用的“已翻译名称”结果。
const metadataNames = ref<InfinityNikkiMetadataNames>({})
// 面板级缓存：key=语言+filter/pose/light 三元组，value=映射结果。
const metadataNamesCache = new Map<string, InfinityNikkiMetadataNames>()

function syncDraftFromProps() {
  codeValueDraft.value = getCodeValue(currentUserRecord.value, codeTypeDraft.value)
}

watch(
  () => props.details,
  () => {
    syncDraftFromProps()
  },
  { deep: true, immediate: true }
)

watch(
  () => props.assetId,
  () => {
    clearSameOutfitDyeFillPreview()
  }
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
  clearSameOutfitDyeFillPreview()
}

function clearSameOutfitDyeFillPreview() {
  sameOutfitDyeFillPreview.value = null
  sameOutfitDyeFillPreviewCodeValue.value = ''
}

function getCodeValue(
  record: InfinityNikkiDetails['userRecord'],
  codeType: InfinityNikkiUserRecordCodeType
): string {
  if (!record) {
    return ''
  }
  return codeType === 'home_building' ? (record.homeBuildingCode ?? '') : (record.dyeCode ?? '')
}

function buildUserRecord(details: {
  dyeCode?: string
  homeBuildingCode?: string
  worldId?: string
}): InfinityNikkiDetails['userRecord'] {
  if (!details.dyeCode && !details.homeBuildingCode && !details.worldId) {
    return undefined
  }
  return {
    dyeCode: details.dyeCode,
    homeBuildingCode: details.homeBuildingCode,
    worldId: details.worldId,
  }
}

function buildUpdatedUserRecordForCode(
  codeType: InfinityNikkiUserRecordCodeType,
  codeValue: string | undefined
): InfinityNikkiDetails['userRecord'] {
  const currentRecord = currentUserRecord.value
  return buildUserRecord({
    dyeCode: codeType === 'dye' ? codeValue : currentRecord?.dyeCode,
    homeBuildingCode: codeType === 'home_building' ? codeValue : currentRecord?.homeBuildingCode,
    worldId: currentRecord?.worldId,
  })
}

async function refreshSameOutfitDyeFillPreview(codeValue: string) {
  clearSameOutfitDyeFillPreview()
  if (!codeValue) {
    return
  }

  try {
    const preview = await previewInfinityNikkiSameOutfitDyeCodeFill({
      assetId: props.assetId,
    })
    sameOutfitDyeFillPreview.value = preview
    sameOutfitDyeFillPreviewCodeValue.value = codeValue
  } catch {
    // 预览失败不影响当前照片登记；用户仍可继续浏览或稍后重试保存。
    clearSameOutfitDyeFillPreview()
  }
}

async function handleUserRecordCommit() {
  if (isSavingUserRecord.value) {
    return
  }

  const normalizedCodeValue = codeValueDraft.value.trim()
  const nextCodeType = codeTypeDraft.value
  const currentCodeValue = getCodeValue(currentUserRecord.value, nextCodeType).trim()
  codeValueDraft.value = normalizedCodeValue

  if (!currentCodeValue && !normalizedCodeValue) {
    return
  }

  if (normalizedCodeValue === currentCodeValue) {
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
      userRecord: buildUpdatedUserRecordForCode(nextCodeType, normalizedCodeValue || undefined),
      mapArea: props.details.mapArea,
    })

    if (nextCodeType === 'dye' && normalizedCodeValue) {
      await refreshSameOutfitDyeFillPreview(normalizedCodeValue)
    } else {
      clearSameOutfitDyeFillPreview()
    }
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

async function handleFillSameOutfitDyeCode() {
  if (isFillingSameOutfitDyeCode.value || !canShowSameOutfitDyeFillPrompt.value) {
    return
  }

  const codeValue = sameOutfitDyeFillPreviewCodeValue.value
  isFillingSameOutfitDyeCode.value = true

  try {
    const result = await fillInfinityNikkiSameOutfitDyeCode({
      assetId: props.assetId,
      codeValue,
    })

    // 覆盖完成后收起提示区，避免重复操作噪音。
    clearSameOutfitDyeFillPreview()

    toast.success(t('gallery.details.infinityNikki.sameOutfitDyeFill.successTitle'), {
      description: t(
        'gallery.details.infinityNikki.sameOutfitDyeFill.successDescriptionOverwriteAll',
        {
          count: result.affectedCount,
          updated: result.updatedExistingCount,
        }
      ),
    })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.details.infinityNikki.sameOutfitDyeFill.failedTitle'), {
      description: message,
    })
  } finally {
    isFillingSameOutfitDyeCode.value = false
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

  await handleUserRecordCommit()
  codeTypeDraft.value = nextCodeType
  syncDraftFromProps()
}

async function handleSelectWorldId(nextWorldId: string | undefined) {
  if (isSavingWorldRecord.value) {
    return
  }

  isWorldPopoverOpen.value = false
  const normalizedWorldId = nextWorldId ? normalizeInfinityNikkiWorldId(nextWorldId) : undefined
  const currentUserWorldId = currentMapArea.value?.userWorldId
    ? normalizeInfinityNikkiWorldId(currentMapArea.value.userWorldId)
    : undefined

  if ((normalizedWorldId ?? '') === (currentUserWorldId ?? '')) {
    return
  }

  isSavingWorldRecord.value = true

  try {
    await setInfinityNikkiWorldRecord({
      assetId: props.assetId,
      worldId: normalizedWorldId,
    })

    const autoWorldId = currentMapArea.value?.autoWorldId
    const nextMapArea =
      autoWorldId && (normalizedWorldId || currentMapArea.value)
        ? {
            autoWorldId,
            userWorldId: normalizedWorldId,
            worldId: normalizedWorldId ?? autoWorldId,
          }
        : props.details.mapArea

    emit('updated', {
      extracted: props.details.extracted,
      userRecord: buildUserRecord({
        dyeCode: currentUserRecord.value?.dyeCode,
        homeBuildingCode: currentUserRecord.value?.homeBuildingCode,
        worldId: normalizedWorldId,
      }),
      mapArea: nextMapArea,
    })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.details.infinityNikki.saveWorldRecordFailed'), {
      description: message,
    })
  } finally {
    isSavingWorldRecord.value = false
  }
}

async function handleOpenMapLocation() {
  const target = currentMapLocationTarget.value
  if (!target) {
    return
  }

  mapStore.setPendingFocusRequest({
    assetId: props.assetId,
    requestId: Date.now(),
    worldId: target.worldId,
  })
  await router.push({
    name: 'map',
  })
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

function normalizeLocaleForMetadata(localeValue: string): 'zh-CN' | 'en-US' {
  // 与后端约定统一只传两种 locale，避免出现多种别名导致缓存碎片。
  return localeValue.startsWith('zh') ? 'zh-CN' : 'en-US'
}

function buildMetadataCacheKey(params: {
  filterId?: number
  poseId?: number
  lightId?: number
  locale: 'zh-CN' | 'en-US'
}): string {
  return `${params.locale}|${params.filterId ?? ''}|${params.poseId ?? ''}|${params.lightId ?? ''}`
}

function formatMetadataDisplay(idText: string | null, name: string | undefined): string | null {
  if (!idText) return null
  // 面向普通用户时，命中字典仅展示语义名称；未命中才回退到原始 ID。
  if (!name) return idText
  return name
}

function formatMetadataTitle(idText: string | null, name: string | undefined): string | undefined {
  if (!idText) return undefined
  // 命中字典时将 ID 放到 title，保留排查能力但不干扰主视觉。
  if (name) return `ID: ${idText}`
  return idText
}

async function refreshMetadataNames() {
  const filterId = extracted.value?.filterId
  const poseId = extracted.value?.poseId
  const lightId = extracted.value?.lightId

  if (filterId === undefined && poseId === undefined && lightId === undefined) {
    // 当前资产没有相关参数时清空展示，避免沿用上一张图的结果。
    metadataNames.value = {}
    return
  }

  const normalizedLocale = normalizeLocaleForMetadata(locale.value)
  const cacheKey = buildMetadataCacheKey({
    filterId,
    poseId,
    lightId,
    locale: normalizedLocale,
  })

  const cached = metadataNamesCache.get(cacheKey)
  if (cached) {
    // 组件级缓存：同一语言+同一组 ID 直接复用，避免重复 RPC。
    metadataNames.value = cached
    return
  }

  const result = await getInfinityNikkiMetadataNames({
    filterId,
    poseId,
    lightId,
    locale: normalizedLocale,
  })
  // 即使 result 为空对象也会缓存，避免失败场景下短时间重复请求。
  metadataNamesCache.set(cacheKey, result)
  metadataNames.value = result
}

watch(
  () => [
    extracted.value?.filterId,
    extracted.value?.poseId,
    extracted.value?.lightId,
    locale.value,
  ],
  () => {
    // 当资产切换或语言切换时刷新映射，保证展示始终与当前上下文一致。
    void refreshMetadataNames()
  },
  { immediate: true }
)
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

      <div
        v-if="canShowSameOutfitDyeFillPrompt"
        class="space-y-1 rounded border border-border/70 bg-muted/30 px-2 py-1.5"
      >
        <div class="text-muted-foreground">
          {{
            t('gallery.details.infinityNikki.sameOutfitDyeFill.prompt', {
              matched: sameOutfitDyeFillPreview?.matchedCount ?? 0,
              recorded: sameOutfitDyeFillPreview?.recordedCount ?? 0,
            })
          }}
        </div>
        <div class="flex items-center justify-end">
          <Button
            variant="outline"
            size="sm"
            class="h-6 px-2 text-xs"
            :disabled="isFillingSameOutfitDyeCode"
            @click="void handleFillSameOutfitDyeCode()"
          >
            {{ t('gallery.details.infinityNikki.sameOutfitDyeFill.actionOverwriteAll') }}
          </Button>
        </div>
      </div>

      <template v-if="extracted">
        <div v-if="formatGameTime(extracted)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.gameTime')
          }}</span>
          <span class="font-mono">{{ formatGameTime(extracted) }}</span>
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
          <span class="font-mono">{{ formatFocalLength(extracted.cameraFocalLength) }}</span>
        </div>
        <div v-if="formatApertureValue(extracted.apertureValue)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.apertureValue')
          }}</span>
          <span class="font-mono">{{ formatApertureValue(extracted.apertureValue) }}</span>
        </div>

        <div v-if="formatSignedNumber(extracted.rotation, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.rotation')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.rotation, 2) }}</span>
        </div>
        <div
          v-if="formatPercentage(extracted.vignetteIntensity)"
          class="flex justify-between gap-2"
        >
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.vignetteIntensity')
          }}</span>
          <span class="font-mono">{{ formatPercentage(extracted.vignetteIntensity) }}</span>
        </div>
        <div v-if="formatPercentage(extracted.bloomIntensity)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.bloomIntensity')
          }}</span>
          <span class="font-mono">{{ formatPercentage(extracted.bloomIntensity) }}</span>
        </div>
        <div
          v-if="formatSignedNumber(extracted.bloomThreshold, 2)"
          class="flex justify-between gap-2"
        >
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.bloomThreshold')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.bloomThreshold, 2) }}</span>
        </div>
        <div v-if="formatPercentage(extracted.brightness)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.brightness')
          }}</span>
          <span class="font-mono">{{ formatPercentage(extracted.brightness) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.exposure, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.exposure')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.exposure, 2) }}</span>
        </div>
        <div v-if="formatPercentage(extracted.contrast)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.contrast')
          }}</span>
          <span class="font-mono">{{ formatPercentage(extracted.contrast) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.saturation, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.saturation')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.saturation, 2) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.vibrance, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.vibrance')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.vibrance, 2) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.highlights, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.highlights')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.highlights, 2) }}</span>
        </div>
        <div v-if="formatSignedNumber(extracted.shadow, 2)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.shadow')
          }}</span>
          <span class="font-mono">{{ formatSignedNumber(extracted.shadow, 2) }}</span>
        </div>
        <div v-if="formatPoseId(extracted.poseId)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.poseId')
          }}</span>
          <span
            class="max-w-32 truncate"
            :class="{ 'font-mono': !metadataNames.poseName }"
            :title="formatMetadataTitle(formatPoseId(extracted.poseId), metadataNames.poseName)"
          >
            {{ formatMetadataDisplay(formatPoseId(extracted.poseId), metadataNames.poseName) }}
          </span>
        </div>
        <div v-if="formatMetadataId(extracted.lightId)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.lightId')
          }}</span>
          <span
            class="max-w-32 truncate"
            :class="{ 'font-mono': !metadataNames.lightName }"
            :title="
              formatMetadataTitle(formatMetadataId(extracted.lightId), metadataNames.lightName)
            "
          >
            {{
              formatMetadataDisplay(formatMetadataId(extracted.lightId), metadataNames.lightName)
            }}
          </span>
        </div>
        <div v-if="formatPercentage(extracted.lightStrength)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.lightStrength')
          }}</span>
          <span class="font-mono">{{ formatPercentage(extracted.lightStrength) }}</span>
        </div>

        <div v-if="currentWorldLabel" class="flex items-center justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.nikkiLocation')
          }}</span>
          <div class="flex min-w-0 items-center gap-1">
            <button
              type="button"
              class="min-w-0 cursor-pointer truncate text-left transition-colors hover:text-primary disabled:cursor-default disabled:text-foreground"
              :disabled="!currentMapLocationTarget"
              @click="handleOpenMapLocation"
            >
              {{ currentWorldLabel }}
            </button>
            <Popover v-model:open="isWorldPopoverOpen">
              <PopoverTrigger as-child>
                <Button
                  variant="ghost"
                  size="icon"
                  class="h-5 w-5 text-muted-foreground"
                  :disabled="isSavingWorldRecord"
                >
                  <ChevronDown class="h-3 w-3" />
                </Button>
              </PopoverTrigger>
              <PopoverContent align="end" class="w-44 p-1">
                <button
                  type="button"
                  class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-xs transition-colors hover:bg-accent"
                  @click="void handleSelectWorldId(undefined)"
                >
                  <span>{{ t('gallery.details.infinityNikki.mapArea.auto') }}</span>
                  <Check v-if="!currentMapArea?.userWorldId" class="h-3.5 w-3.5" />
                </button>
                <button
                  v-for="world in INFINITY_NIKKI_WORLD_OPTIONS"
                  :key="world.id"
                  type="button"
                  class="flex w-full items-center justify-between rounded px-2 py-1.5 text-left text-xs transition-colors hover:bg-accent"
                  @click="void handleSelectWorldId(world.id)"
                >
                  <span>{{ getInfinityNikkiWorldName(world.id, locale) }}</span>
                  <Check
                    v-if="normalizeInfinityNikkiWorldId(currentMapArea?.userWorldId) === world.id"
                    class="h-3.5 w-3.5"
                  />
                </button>
              </PopoverContent>
            </Popover>
          </div>
        </div>
        <div v-if="formatMetadataId(extracted.filterId)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.filterId')
          }}</span>
          <span
            class="max-w-32 truncate"
            :class="{ 'font-mono': !metadataNames.filterName }"
            :title="
              formatMetadataTitle(formatMetadataId(extracted.filterId), metadataNames.filterName)
            "
          >
            {{
              formatMetadataDisplay(formatMetadataId(extracted.filterId), metadataNames.filterName)
            }}
          </span>
        </div>
        <div v-if="formatPercentage(extracted.filterStrength)" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.filterStrength')
          }}</span>
          <span class="font-mono">{{ formatPercentage(extracted.filterStrength) }}</span>
        </div>
        <div v-if="extracted.nikkiHidden !== undefined" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.nikkiHidden')
          }}</span>
          <span>{{ extracted.nikkiHidden ? t('common.yes') : t('common.no') }}</span>
        </div>
        <div v-if="extracted.vertical !== undefined" class="flex justify-between gap-2">
          <span class="shrink-0 whitespace-nowrap text-muted-foreground">{{
            t('gallery.details.infinityNikki.vertical')
          }}</span>
          <span>{{ extracted.vertical ? t('common.yes') : t('common.no') }}</span>
        </div>
      </template>
    </div>
  </div>
</template>
