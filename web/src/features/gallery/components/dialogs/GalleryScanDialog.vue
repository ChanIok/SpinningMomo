<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useGalleryData } from '../../composables/useGalleryData'
import type { ScanAssetsParams, ScanIgnoreRule } from '../../types'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Switch } from '@/components/ui/switch'
import { Textarea } from '@/components/ui/textarea'
import { ChevronDown, ChevronUp, Loader2, Plus, Trash2 } from 'lucide-vue-next'

interface Props {
  open: boolean
  preset?: Partial<ScanAssetsParams>
}

interface FormIgnoreRule {
  id: number
  pattern: string
  patternType: 'glob' | 'regex'
  ruleType: 'exclude' | 'include'
  description: string
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
}>()

// 与后端 ScanCommon::default_supported_extensions 保持一致，避免 UI 默认与可扫范围脱节。
const defaultSupportedExtensions = [
  '.jpg',
  '.jpeg',
  '.png',
  '.bmp',
  '.webp',
  '.tiff',
  '.tif',
  '.mp4',
  '.avi',
  '.mov',
  '.mkv',
  '.wmv',
  '.webm',
]

const galleryData = useGalleryData()
const { toast } = useToast()
const { t } = useI18n()

const isSelectingScanDirectory = ref(false)
const isSubmittingScanTask = ref(false)
const showAdvancedOptions = ref(false)
const scanDirectory = ref('')
const generateThumbnails = ref(true)
const thumbnailShortEdge = ref(480)
const supportedExtensionsText = ref(defaultSupportedExtensions.join(', '))
const ignoreRules = ref<FormIgnoreRule[]>([])
const nextIgnoreRuleId = ref(1)

const canSubmitAddFolder = computed(() => {
  return scanDirectory.value.trim().length > 0 && !isSubmittingScanTask.value
})

function toFormIgnoreRules(rules: ScanIgnoreRule[] | undefined): FormIgnoreRule[] {
  if (!rules || rules.length === 0) {
    return []
  }

  return rules.map((rule, index) => ({
    id: index + 1,
    pattern: rule.pattern,
    patternType: rule.patternType ?? 'regex',
    ruleType: rule.ruleType ?? 'exclude',
    description: rule.description ?? '',
  }))
}

function resetForm() {
  scanDirectory.value = ''
  generateThumbnails.value = true
  thumbnailShortEdge.value = 480
  supportedExtensionsText.value = defaultSupportedExtensions.join(', ')
  ignoreRules.value = []
  nextIgnoreRuleId.value = 1
  showAdvancedOptions.value = false
}

function initializeFormFromPreset() {
  resetForm()

  if (props.preset?.directory) {
    scanDirectory.value = props.preset.directory
  }
  if (props.preset?.generateThumbnails !== undefined) {
    generateThumbnails.value = props.preset.generateThumbnails
  }
  if (props.preset?.thumbnailShortEdge !== undefined) {
    thumbnailShortEdge.value = props.preset.thumbnailShortEdge
  }
  if (props.preset?.supportedExtensions && props.preset.supportedExtensions.length > 0) {
    supportedExtensionsText.value = props.preset.supportedExtensions.join(', ')
  }

  ignoreRules.value = toFormIgnoreRules(props.preset?.ignoreRules)
  nextIgnoreRuleId.value = ignoreRules.value.length + 1
  showAdvancedOptions.value = false
}

watch(
  () => props.open,
  (open) => {
    if (open) {
      initializeFormFromPreset()
      return
    }
    if (!isSubmittingScanTask.value) {
      resetForm()
    }
  }
)

function handleDialogOpenChange(open: boolean) {
  if (!open && isSubmittingScanTask.value) {
    return
  }
  emit('update:open', open)
}

function addIgnoreRule() {
  ignoreRules.value.push({
    id: nextIgnoreRuleId.value++,
    pattern: '',
    patternType: 'regex',
    ruleType: 'exclude',
    description: '',
  })
}

function removeIgnoreRule(ruleId: number) {
  ignoreRules.value = ignoreRules.value.filter((rule) => rule.id !== ruleId)
}

function parseSupportedExtensions(input: string): string[] | undefined {
  const tokens = input.split(/[\s,;，；\n]+/).map((token) => token.trim())

  const normalized: string[] = []
  const seen = new Set<string>()

  for (const token of tokens) {
    if (!token) {
      continue
    }

    const extension = (token.startsWith('.') ? token : `.${token}`).toLowerCase()
    if (extension.length <= 1 || seen.has(extension)) {
      continue
    }

    seen.add(extension)
    normalized.push(extension)
  }

  return normalized.length > 0 ? normalized : undefined
}

function buildScanIgnoreRules(): ScanIgnoreRule[] | undefined {
  const rules = ignoreRules.value.reduce<ScanIgnoreRule[]>((acc, rule) => {
    const pattern = rule.pattern.trim()
    if (!pattern) {
      return acc
    }

    acc.push({
      pattern,
      patternType: rule.patternType,
      ruleType: rule.ruleType,
      description: rule.description.trim() || undefined,
    })

    return acc
  }, [])

  return rules.length > 0 ? rules : undefined
}

async function handleSelectScanDirectory() {
  isSelectingScanDirectory.value = true

  try {
    const parentWindowMode = isWebView() ? 1 : 2
    const result = await call<{ path: string }>(
      'dialog.openDirectory',
      {
        title: t('gallery.sidebar.scan.selectDialogTitle'),
        parentWindowMode,
      },
      0
    )

    if (result.path) {
      scanDirectory.value = result.path
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    if (message.toLowerCase().includes('cancel')) {
      return
    }

    toast.error(t('gallery.sidebar.scan.selectDirectoryFailed'), { description: message })
  } finally {
    isSelectingScanDirectory.value = false
  }
}

async function handleImportAlbum() {
  const directory = scanDirectory.value.trim()
  if (!directory) {
    toast.error(t('gallery.sidebar.scan.selectDirectoryRequired'))
    return
  }

  isSubmittingScanTask.value = true
  const loadingToastId = toast.loading(t('gallery.sidebar.scan.submitting'))

  try {
    const scanParams: ScanAssetsParams = {
      directory,
      generateThumbnails: generateThumbnails.value,
      thumbnailShortEdge: thumbnailShortEdge.value,
      supportedExtensions: parseSupportedExtensions(supportedExtensionsText.value),
      ignoreRules: buildScanIgnoreRules(),
    }

    const result = await galleryData.startScanAssets(scanParams)

    toast.dismiss(loadingToastId)
    toast.success(t('gallery.sidebar.scan.queuedTitle'), {
      description: t('gallery.sidebar.scan.queuedDescription', {
        taskId: result.taskId,
      }),
    })

    emit('update:open', false)
    resetForm()
  } catch (error) {
    toast.dismiss(loadingToastId)
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.scan.failedTitle'), { description: message })
  } finally {
    isSubmittingScanTask.value = false
  }
}
</script>

<template>
  <Dialog :open="open" @update:open="handleDialogOpenChange">
    <DialogContent class="overflow-hidden p-0 sm:max-w-[720px]" :show-close-button="false">
      <div class="flex h-full max-h-[85vh] flex-col">
        <DialogHeader class="px-6 pt-6 pb-3">
          <DialogTitle>{{ t('gallery.sidebar.scan.dialogTitle') }}</DialogTitle>
          <DialogDescription>
            {{ t('gallery.sidebar.scan.dialogDescription') }}
          </DialogDescription>
        </DialogHeader>

        <ScrollArea class="min-h-0 flex-1 px-6">
          <div class="space-y-4 pb-4">
            <div class="space-y-2">
              <Label for="scan-directory">{{ t('gallery.sidebar.scan.directoryLabel') }}</Label>
              <div class="flex items-center gap-2">
                <Input
                  id="scan-directory"
                  v-model="scanDirectory"
                  :placeholder="t('gallery.sidebar.scan.directoryPlaceholder')"
                  readonly
                />
                <Button
                  variant="outline"
                  :disabled="isSelectingScanDirectory || isSubmittingScanTask"
                  @click="handleSelectScanDirectory"
                >
                  <Loader2 v-if="isSelectingScanDirectory" class="mr-2 h-4 w-4 animate-spin" />
                  {{
                    isSelectingScanDirectory
                      ? t('gallery.sidebar.scan.selectingDirectory')
                      : t('gallery.sidebar.scan.selectDirectory')
                  }}
                </Button>
              </div>
            </div>

            <button
              type="button"
              class="flex w-full items-center justify-between rounded-md border px-3 py-2 text-left text-sm transition-colors hover:bg-accent/40"
              @click="showAdvancedOptions = !showAdvancedOptions"
            >
              <span>{{ t('gallery.sidebar.scan.advancedOptions') }}</span>
              <ChevronUp v-if="showAdvancedOptions" class="h-4 w-4" />
              <ChevronDown v-else class="h-4 w-4" />
            </button>

            <div v-if="showAdvancedOptions" class="space-y-4 rounded-md border p-3">
              <div class="flex items-center justify-between rounded-md border p-3">
                <div class="space-y-1">
                  <Label>{{ t('gallery.sidebar.scan.generateThumbnails') }}</Label>
                  <p class="text-xs text-muted-foreground">
                    {{ t('gallery.sidebar.scan.generateThumbnailsHint') }}
                  </p>
                </div>
                <Switch
                  :model-value="generateThumbnails"
                  @update:model-value="generateThumbnails = Boolean($event)"
                />
              </div>

              <div class="space-y-2">
                <Label for="thumbnail-short-edge">{{
                  t('gallery.sidebar.scan.thumbnailShortEdge')
                }}</Label>
                <Input
                  id="thumbnail-short-edge"
                  v-model.number="thumbnailShortEdge"
                  type="number"
                  :min="64"
                  :max="4096"
                  :disabled="!generateThumbnails"
                />
              </div>

              <div class="space-y-2">
                <Label for="supported-extensions">{{
                  t('gallery.sidebar.scan.supportedExtensions')
                }}</Label>
                <Textarea
                  id="supported-extensions"
                  v-model="supportedExtensionsText"
                  :rows="3"
                  placeholder=".jpg, .jpeg, .png"
                />
                <p class="text-xs text-muted-foreground">
                  {{ t('gallery.sidebar.scan.supportedExtensionsHint') }}
                </p>
              </div>

              <div class="space-y-2">
                <div class="flex items-center justify-between">
                  <Label>{{ t('gallery.sidebar.scan.ignoreRules') }}</Label>
                  <Button type="button" variant="outline" size="sm" @click="addIgnoreRule">
                    <Plus class="mr-1 h-3 w-3" />
                    {{ t('gallery.sidebar.scan.addRule') }}
                  </Button>
                </div>

                <div
                  v-if="ignoreRules.length === 0"
                  class="rounded-md border border-dashed p-3 text-xs text-muted-foreground"
                >
                  {{ t('gallery.sidebar.scan.noRules') }}
                </div>

                <div
                  v-for="rule in ignoreRules"
                  :key="rule.id"
                  class="space-y-3 rounded-md border p-3"
                >
                  <div class="flex items-start justify-between gap-2">
                    <div class="grid flex-1 gap-3 sm:grid-cols-2">
                      <div class="space-y-2 sm:col-span-2">
                        <Label>{{ t('gallery.sidebar.scan.rulePattern') }}</Label>
                        <Input
                          v-model="rule.pattern"
                          :placeholder="t('gallery.sidebar.scan.rulePatternPlaceholder')"
                        />
                      </div>

                      <div class="space-y-2">
                        <Label>{{ t('gallery.sidebar.scan.patternType') }}</Label>
                        <select
                          v-model="rule.patternType"
                          class="flex h-9 w-full rounded-md border border-input bg-transparent px-3 py-1 text-sm shadow-xs transition-[color,box-shadow] outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50"
                        >
                          <option value="regex">
                            {{ t('gallery.sidebar.scan.patternTypeRegex') }}
                          </option>
                          <option value="glob">
                            {{ t('gallery.sidebar.scan.patternTypeGlob') }}
                          </option>
                        </select>
                      </div>

                      <div class="space-y-2">
                        <Label>{{ t('gallery.sidebar.scan.ruleType') }}</Label>
                        <select
                          v-model="rule.ruleType"
                          class="flex h-9 w-full rounded-md border border-input bg-transparent px-3 py-1 text-sm shadow-xs transition-[color,box-shadow] outline-none focus-visible:border-ring focus-visible:ring-[3px] focus-visible:ring-ring/50"
                        >
                          <option value="exclude">
                            {{ t('gallery.sidebar.scan.ruleTypeExclude') }}
                          </option>
                          <option value="include">
                            {{ t('gallery.sidebar.scan.ruleTypeInclude') }}
                          </option>
                        </select>
                      </div>

                      <div class="space-y-2 sm:col-span-2">
                        <Label>{{ t('gallery.sidebar.scan.ruleDescription') }}</Label>
                        <Input
                          v-model="rule.description"
                          :placeholder="t('gallery.sidebar.scan.ruleDescriptionPlaceholder')"
                        />
                      </div>
                    </div>

                    <Button
                      type="button"
                      variant="ghost"
                      size="icon"
                      class="h-8 w-8"
                      @click="removeIgnoreRule(rule.id)"
                    >
                      <Trash2 class="h-4 w-4 text-destructive" />
                    </Button>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </ScrollArea>

        <DialogFooter class="shrink-0 border-t px-6 py-4">
          <Button
            variant="outline"
            :disabled="isSubmittingScanTask"
            @click="handleDialogOpenChange(false)"
          >
            {{ t('gallery.sidebar.scan.cancel') }}
          </Button>
          <Button :disabled="!canSubmitAddFolder" @click="handleImportAlbum">
            <Loader2 v-if="isSubmittingScanTask" class="mr-2 h-4 w-4 animate-spin" />
            {{
              isSubmittingScanTask
                ? t('gallery.sidebar.scan.submitting')
                : t('gallery.sidebar.scan.submit')
            }}
          </Button>
        </DialogFooter>
      </div>
    </DialogContent>
  </Dialog>
</template>
