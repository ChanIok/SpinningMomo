<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
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
import { Separator } from '@/components/ui/separator'
import { Switch } from '@/components/ui/switch'
import { Textarea } from '@/components/ui/textarea'
import { ChevronDown, ChevronUp, Loader2, Plus, Trash2 } from 'lucide-vue-next'
import { useGallerySidebar, useGalleryData } from '../composables'
import { useGalleryStore } from '../store'
import type { FolderTreeNode, ScanAssetsParams, ScanIgnoreRule } from '../types'
import FolderTreeItem from '../components/FolderTreeItem.vue'
import TagTreeItem from '../components/TagTreeItem.vue'
import TagInlineEditor from '../components/TagInlineEditor.vue'

const galleryData = useGalleryData()
const galleryStore = useGalleryStore()
const { toast } = useToast()
const { t } = useI18n()

const {
  folders,
  foldersLoading,
  foldersError,
  tags,
  tagsLoading,
  tagsError,
  selectedFolder,
  selectedTag,
  selectFolder,
  clearFolderFilter,
  updateFolderDisplayName,
  openFolderInExplorer,
  removeFolderWatch,
  clearTagFilter,
  selectTag,
  loadTagTree,
  createTag,
  updateTag,
  deleteTag,
} = useGallerySidebar()

// 标签创建状态
const isCreatingTag = ref(false)

interface FormIgnoreRule {
  id: number
  pattern: string
  patternType: 'glob' | 'regex'
  ruleType: 'exclude' | 'include'
  description: string
}

const defaultSupportedExtensions = ['.jpg', '.jpeg', '.png', '.bmp', '.webp', '.tiff', '.tif']

// 文件夹扫描弹窗状态
const showAddFolderDialog = ref(false)
const isSelectingScanDirectory = ref(false)
const isScanningDirectory = ref(false)
const showAdvancedOptions = ref(false)
const scanDirectory = ref('')
const generateThumbnails = ref(true)
const thumbnailShortEdge = ref(480)
const supportedExtensionsText = ref(defaultSupportedExtensions.join(', '))
const ignoreRules = ref<FormIgnoreRule[]>([])
const nextIgnoreRuleId = ref(1)

const canSubmitAddFolder = computed(() => {
  return scanDirectory.value.trim().length > 0 && !isScanningDirectory.value
})

const isFolderTitleSelected = computed(() => {
  return galleryStore.sidebar.activeSection === 'folders' && selectedFolder.value === null
})

const isTagTitleSelected = computed(() => {
  return galleryStore.sidebar.activeSection === 'tags' && selectedTag.value === null
})

function resetAddFolderForm() {
  scanDirectory.value = ''
  generateThumbnails.value = true
  thumbnailShortEdge.value = 480
  supportedExtensionsText.value = defaultSupportedExtensions.join(', ')
  ignoreRules.value = []
  nextIgnoreRuleId.value = 1
  showAdvancedOptions.value = false
}

function startAddFolder() {
  resetAddFolderForm()
  showAddFolderDialog.value = true
}

function handleAddFolderDialogOpenChange(open: boolean) {
  if (!open && isScanningDirectory.value) {
    return
  }

  showAddFolderDialog.value = open

  if (!open) {
    resetAddFolderForm()
  }
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

async function handleAddAndScanFolder() {
  const directory = scanDirectory.value.trim()
  if (!directory) {
    toast.error(t('gallery.sidebar.scan.selectDirectoryRequired'))
    return
  }

  isScanningDirectory.value = true
  const loadingToastId = toast.loading(t('gallery.sidebar.scan.loading'))

  try {
    const scanParams: ScanAssetsParams = {
      directory,
      generateThumbnails: generateThumbnails.value,
      thumbnailShortEdge: thumbnailShortEdge.value,
      supportedExtensions: parseSupportedExtensions(supportedExtensionsText.value),
      ignoreRules: buildScanIgnoreRules(),
    }

    const result = await galleryData.scanAssets(scanParams)

    await galleryData.loadFolderTree()
    if (galleryStore.isTimelineMode) {
      await galleryData.loadTimelineData()
    } else {
      await galleryData.loadAllAssets()
    }

    toast.dismiss(loadingToastId)
    toast.success(t('gallery.sidebar.scan.successTitle'), {
      description: t('gallery.sidebar.scan.successDescription', {
        totalFiles: result.totalFiles,
        newItems: result.newItems,
        updatedItems: result.updatedItems,
      }),
    })

    if (result.errors.length > 0) {
      toast.warning(t('gallery.sidebar.scan.partialErrorsTitle'), {
        description: result.errors.slice(0, 3).join('; '),
      })
    }

    showAddFolderDialog.value = false
    resetAddFolderForm()
  } catch (error) {
    toast.dismiss(loadingToastId)
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.scan.failedTitle'), { description: message })
  } finally {
    isScanningDirectory.value = false
  }
}

function startCreateTag() {
  isCreatingTag.value = true
}

async function handleCreateTag(name: string) {
  try {
    await createTag(name)
    isCreatingTag.value = false
  } catch (error) {
    console.error('Failed to create tag:', error)
  }
}

function handleCancelCreateTag() {
  isCreatingTag.value = false
}

async function handleRenameTag(tagId: number, newName: string) {
  try {
    await updateTag(tagId, newName)
  } catch (error) {
    console.error('Failed to rename tag:', error)
  }
}

async function handleCreateChildTag(parentId: number, name: string) {
  try {
    await createTag(name, parentId)
  } catch (error) {
    console.error('Failed to create child tag:', error)
  }
}

async function handleDeleteTag(tagId: number) {
  try {
    await deleteTag(tagId)
  } catch (error) {
    console.error('Failed to delete tag:', error)
  }
}

function folderExistsById(nodes: FolderTreeNode[], folderId: number): boolean {
  for (const node of nodes) {
    if (node.id === folderId) {
      return true
    }
    if (node.children && folderExistsById(node.children, folderId)) {
      return true
    }
  }
  return false
}

async function handleRenameFolderDisplayName(folderId: number, displayName: string) {
  try {
    await updateFolderDisplayName(folderId, displayName)
    await galleryData.loadFolderTree()

    if (selectedFolder.value === folderId) {
      const folderName = displayName.trim()
      selectFolder(folderId, folderName)
    }
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.rename.failedTitle'), { description: message })
  }
}

async function handleOpenFolderInExplorer(folderId: number) {
  try {
    await openFolderInExplorer(folderId)
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.openInExplorer.failedTitle'), { description: message })
  }
}

async function handleRemoveFolderWatch(folderId: number) {
  try {
    await removeFolderWatch(folderId)

    await galleryData.loadFolderTree()

    const currentSelectedFolderId = selectedFolder.value
    if (
      currentSelectedFolderId !== null &&
      !folderExistsById(galleryStore.folders, currentSelectedFolderId)
    ) {
      clearFolderFilter()
    }

    if (galleryStore.isTimelineMode) {
      await galleryData.loadTimelineData()
    } else {
      await galleryData.loadAllAssets()
    }

    toast.success(t('gallery.sidebar.folders.removeWatch.successTitle'), {
      description: t('gallery.sidebar.folders.removeWatch.successDescription'),
    })
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.sidebar.folders.removeWatch.failedTitle'), { description: message })
  }
}

onMounted(() => {
  galleryData.loadFolderTree()
  loadTagTree()
})
</script>

<template>
  <div class="flex h-full flex-col">
    <!-- 导航菜单 -->
    <div class="flex-1 overflow-auto p-4">
      <!-- 文件夹区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <button
            type="button"
            :class="[
              'cursor-pointer rounded px-2 py-1 text-xs font-medium tracking-wider uppercase transition-colors',
              isFolderTitleSelected
                ? 'bg-accent text-accent-foreground'
                : 'text-muted-foreground hover:text-foreground',
            ]"
            @click="clearFolderFilter"
          >
            {{ t('gallery.sidebar.folders.title') }}
          </button>
          <Button variant="ghost" size="icon" class="h-6 w-6" @click="startAddFolder">
            <Plus class="h-3 w-3" />
          </Button>
        </div>
        <!-- 加载状态 -->
        <div v-if="foldersLoading" class="px-2 text-xs text-muted-foreground">
          {{ t('gallery.sidebar.common.loading') }}
        </div>
        <!-- 错误状态 -->
        <div v-else-if="foldersError" class="px-2 text-xs text-destructive">
          {{ foldersError }}
        </div>
        <!-- 文件夹树 -->
        <div v-else class="space-y-1">
          <FolderTreeItem
            v-for="folder in folders"
            :key="folder.id"
            :folder="folder"
            :selected-folder="selectedFolder"
            :depth="0"
            @select="selectFolder"
            @rename-display-name="handleRenameFolderDisplayName"
            @open-in-explorer="handleOpenFolderInExplorer"
            @remove-watch="handleRemoveFolderWatch"
          />
        </div>
      </div>

      <Separator class="my-4" />

      <!-- 标签区域 -->
      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <button
            type="button"
            :class="[
              'cursor-pointer rounded px-2 py-1 text-xs font-medium tracking-wider uppercase transition-colors',
              isTagTitleSelected
                ? 'bg-accent text-accent-foreground'
                : 'text-muted-foreground hover:text-foreground',
            ]"
            @click="clearTagFilter"
          >
            {{ t('gallery.sidebar.tags.title') }}
          </button>
          <Button variant="ghost" size="icon" class="h-6 w-6" @click="startCreateTag">
            <Plus class="h-3 w-3" />
          </Button>
        </div>
        <!-- 加载状态 -->
        <div v-if="tagsLoading" class="px-2 text-xs text-muted-foreground">
          {{ t('gallery.sidebar.common.loading') }}
        </div>
        <!-- 错误状态 -->
        <div v-else-if="tagsError" class="px-2 text-xs text-destructive">
          {{ tagsError }}
        </div>
        <!-- 标签树 -->
        <div v-else class="space-y-1">
          <!-- 快速创建标签 -->
          <div v-if="isCreatingTag" class="px-2">
            <TagInlineEditor
              :placeholder="t('gallery.sidebar.tags.createPlaceholder')"
              @confirm="handleCreateTag"
              @cancel="handleCancelCreateTag"
            />
          </div>
          <!-- 标签列表 -->
          <TagTreeItem
            v-for="tag in tags"
            :key="tag.id"
            :tag="tag"
            :selected-tag="selectedTag"
            :depth="0"
            @select="selectTag"
            @rename="handleRenameTag"
            @create-child="handleCreateChildTag"
            @delete="handleDeleteTag"
          />
        </div>
      </div>
    </div>

    <Dialog :open="showAddFolderDialog" @update:open="handleAddFolderDialogOpenChange">
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
                    :disabled="isSelectingScanDirectory || isScanningDirectory"
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
              :disabled="isScanningDirectory"
              @click="handleAddFolderDialogOpenChange(false)"
            >
              {{ t('gallery.sidebar.scan.cancel') }}
            </Button>
            <Button :disabled="!canSubmitAddFolder" @click="handleAddAndScanFolder">
              <Loader2 v-if="isScanningDirectory" class="mr-2 h-4 w-4 animate-spin" />
              {{
                isScanningDirectory
                  ? t('gallery.sidebar.scan.scanning')
                  : t('gallery.sidebar.scan.submit')
              }}
            </Button>
          </DialogFooter>
        </div>
      </DialogContent>
    </Dialog>
  </div>
</template>
