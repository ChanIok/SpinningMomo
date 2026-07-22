<script setup lang="ts">
import { computed, ref, watch, Transition } from 'vue'
import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { useGalleryData } from '../../composables/useGalleryData'
import type { ScanAssetsParams, ScanIgnoreRule } from '../../types'
import { useSettingsStore } from '../../../settings/store'
import { storeToRefs } from 'pinia'
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { ChevronRight, Loader2, Plus, Trash2 } from 'lucide-vue-next'

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

const settingsStore = useSettingsStore()
const { runtimeCapabilities } = storeToRefs(settingsStore)

/** 是否为 Windows 盘符根路径（如 C:\\、D:/），用于避免将输出目录设为整盘根。 */
const isWindowsDriveRoot = (raw: string) => {
  const normalized = raw.trim().replace(/\\/g, '/')
  return /^[A-Za-z]:\/?$/.test(normalized)
}

/** 判断 target 路径是否在 base 路径内部（不敏感大小写，支持斜杠归一化） */
const isPathWithinBase = (target: string, base: string): boolean => {
  let normTarget = target.replace(/\\/g, '/').toLowerCase()
  let normBase = base.replace(/\\/g, '/').toLowerCase()

  if (normTarget.endsWith('/') && normTarget.length > 3) {
    normTarget = normTarget.slice(0, -1)
  }
  if (normBase.endsWith('/') && normBase.length > 3) {
    normBase = normBase.slice(0, -1)
  }

  if (!normTarget.startsWith(normBase)) {
    return false
  }
  if (normTarget.length === normBase.length) {
    return true
  }

  return normTarget[normBase.length] === '/' || normBase.endsWith('/')
}

const isSubmittingScanTask = ref(false)
const showAdvancedOptions = ref(false)
const scanDirectory = ref('')
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
      // 1. 磁盘根目录校验
      if (isWindowsDriveRoot(result.path)) {
        toast.warning(t('settings.function.outputDir.driveRootNotAllowedTitle'), {
          description: t('settings.function.outputDir.driveRootNotAllowedDescription'),
        })
        return
      }

      // 2. 程序数据目录校验 (防止 Thumbnails 循环监听)
      const appDataDir = runtimeCapabilities.value?.appDataDir
      if (appDataDir && isPathWithinBase(appDataDir, result.path)) {
        toast.warning(t('settings.function.outputDir.invalidDirTitle'), {
          description: t('settings.function.outputDir.appDataNotAllowedDescription'),
        })
        return
      }

      scanDirectory.value = result.path
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    if (message.toLowerCase().includes('cancel')) {
      return
    }

    toast.error(t('gallery.sidebar.scan.selectDirectoryFailed'), { description: message })
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

function handleExpandEnter(el: Element) {
  const target = el as HTMLElement
  const height = target.scrollHeight
  target.style.height = '0px'
  requestAnimationFrame(() => {
    target.style.height = `${height}px`
  })
}

function handleExpandAfterEnter(el: Element) {
  const target = el as HTMLElement
  target.style.height = 'auto'
}

function handleExpandLeave(el: Element) {
  const target = el as HTMLElement
  target.style.height = `${target.scrollHeight}px`
  requestAnimationFrame(() => {
    target.style.height = '0px'
  })
}
</script>

<template>
  <Dialog :open="open" @update:open="handleDialogOpenChange">
    <DialogContent class="overflow-hidden p-0 sm:max-w-xl" :show-close-button="false">
      <div class="flex h-full max-h-[85vh] flex-col">
        <DialogHeader class="px-6 pt-6 pb-3">
          <DialogTitle>{{ t('gallery.sidebar.scan.dialogTitle') }}</DialogTitle>
        </DialogHeader>

        <ScrollArea class="min-h-0 flex-1 px-6">
          <div class="space-y-5 py-2">
            <div class="space-y-2">
              <Label for="scan-directory" class="text-sm font-medium">{{
                t('gallery.sidebar.scan.directoryLabel')
              }}</Label>
              <div class="flex items-center gap-2">
                <Input
                  id="scan-directory"
                  v-model="scanDirectory"
                  class="flex-1"
                  :placeholder="t('gallery.sidebar.scan.directoryPlaceholder')"
                  readonly
                />
                <Button
                  variant="outline"
                  :disabled="isSubmittingScanTask"
                  @click="handleSelectScanDirectory"
                >
                  {{ t('gallery.sidebar.scan.selectDirectory') }}
                </Button>
              </div>
            </div>

            <div class="space-y-1">
              <button
                type="button"
                class="flex cursor-pointer items-center gap-1.5 text-sm font-medium text-foreground transition-colors hover:opacity-80"
                @click="showAdvancedOptions = !showAdvancedOptions"
              >
                <ChevronRight
                  class="h-4 w-4 text-muted-foreground transition-transform duration-200"
                  :class="{ 'rotate-90': showAdvancedOptions }"
                />
                <span>{{ t('gallery.sidebar.scan.advancedOptions') }}</span>
              </button>
              <p class="pl-5.5 text-xs text-muted-foreground">
                {{ t('gallery.sidebar.scan.advancedOptionsHint') }}
              </p>
            </div>

            <Transition
              name="dialog-expand"
              @enter="handleExpandEnter"
              @after-enter="handleExpandAfterEnter"
              @leave="handleExpandLeave"
            >
              <div v-if="showAdvancedOptions" class="space-y-4 pt-2">
                <div class="space-y-2">
                  <Label for="supported-extensions" class="text-sm font-medium">{{
                    t('gallery.sidebar.scan.supportedExtensions')
                  }}</Label>
                  <Input
                    id="supported-extensions"
                    v-model="supportedExtensionsText"
                    placeholder=".jpg, .jpeg, .png"
                  />
                </div>

                <div class="space-y-2">
                  <div class="flex items-center justify-between">
                    <Label class="text-sm font-medium">{{
                      t('gallery.sidebar.scan.ignoreRules')
                    }}</Label>
                    <Button
                      type="button"
                      variant="outline"
                      size="sm"
                      class="h-9 text-xs"
                      @click="addIgnoreRule"
                    >
                      <Plus class="mr-0 h-3 w-3" />
                      {{ t('gallery.sidebar.scan.addRule') }}
                    </Button>
                  </div>

                  <!-- 显式表格头，确保对齐与清晰指引 -->
                  <div
                    v-if="ignoreRules.length > 0"
                    class="grid grid-cols-[1fr_100px_105px_36px] gap-2 text-[11px] font-medium text-muted-foreground"
                  >
                    <span class="px-2">{{ t('gallery.sidebar.scan.rulePattern') }}</span>
                    <span class="px-2">{{ t('gallery.sidebar.scan.patternType') }}</span>
                    <span class="px-2">{{ t('gallery.sidebar.scan.ruleType') }}</span>
                    <span></span>
                  </div>

                  <div v-if="ignoreRules.length === 0" class="py-1 text-xs text-muted-foreground">
                    {{ t('gallery.sidebar.scan.noRules') }}
                  </div>

                  <div v-else class="space-y-2">
                    <div
                      v-for="rule in ignoreRules"
                      :key="rule.id"
                      class="grid grid-cols-[1fr_100px_105px_36px] items-center gap-2"
                    >
                      <Input
                        v-model="rule.pattern"
                        :placeholder="t('gallery.sidebar.scan.rulePatternPlaceholder')"
                      />
                      <Select v-model="rule.patternType">
                        <SelectTrigger class="w-full">
                          <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                          <SelectItem value="regex">regex</SelectItem>
                          <SelectItem value="glob">glob</SelectItem>
                        </SelectContent>
                      </Select>
                      <Select v-model="rule.ruleType">
                        <SelectTrigger class="w-full">
                          <SelectValue />
                        </SelectTrigger>
                        <SelectContent>
                          <SelectItem value="exclude">exclude</SelectItem>
                          <SelectItem value="include">include</SelectItem>
                        </SelectContent>
                      </Select>
                      <Button
                        type="button"
                        variant="ghost"
                        size="icon"
                        class="h-9 w-9 shrink-0 text-muted-foreground hover:text-destructive"
                        @click="removeIgnoreRule(rule.id)"
                      >
                        <Trash2 class="h-4 w-4" />
                      </Button>
                    </div>
                  </div>
                </div>
              </div>
            </Transition>
          </div>
        </ScrollArea>

        <DialogFooter class="shrink-0 px-6 py-4">
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

<style scoped>
.dialog-expand-enter-active,
.dialog-expand-leave-active {
  transition: height 0.2s ease-out;
  overflow: hidden;
}
</style>
