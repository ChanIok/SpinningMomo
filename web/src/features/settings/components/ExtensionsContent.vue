<script setup lang="ts">
import { computed, ref } from 'vue'
import { storeToRefs } from 'pinia'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Switch } from '@/components/ui/switch'
import {
  Item,
  ItemActions,
  ItemContent,
  ItemDescription,
  ItemGroup,
  ItemTitle,
} from '@/components/ui/item'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { detectInfinityNikkiGameDirectory, getFileInfo, selectDirectory } from '../api'
import { useExtensionActions } from '../composables/useExtensionActions'
import { useSettingsStore } from '../store'
import ResetSettingsDialog from './ResetSettingsDialog.vue'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const { clearError } = store
const { t } = useI18n()
const { toast } = useToast()
const {
  updateInfinityNikkiEnabled,
  updateInfinityNikkiGameDir,
  updateInfinityNikkiAllowOnlinePhotoMetadataExtract,
  updateInfinityNikkiManageScreenshotHardlinks,
  completeInfinityNikkiInitialization,
  resetExtensionSettings,
} = useExtensionActions()

const isDetectingGameDir = ref(false)
const isSelectingGameDir = ref(false)
const isCompletingInitialization = ref(false)

type InitializationStatus = {
  badgeVariant: 'default' | 'secondary' | 'outline'
  badgeLabel: string
  description: string
}

const infinityNikkiSettings = computed(() => appSettings.value.extensions.infinityNikki)
const trimmedGameDir = computed(() => infinityNikkiSettings.value.gameDir.trim())
const canCompleteInitialization = computed(() => {
  return infinityNikkiSettings.value.enable && trimmedGameDir.value.length > 0
})
const initializationStatus = computed<InitializationStatus>(() => {
  if (infinityNikkiSettings.value.galleryGuideSeen) {
    return {
      badgeVariant: 'secondary',
      badgeLabel: t('settings.extensions.infinityNikki.initialization.completed'),
      description: t('settings.extensions.infinityNikki.initialization.completedDescription'),
    }
  }

  if (!canCompleteInitialization.value) {
    return {
      badgeVariant: 'outline',
      badgeLabel: t('settings.extensions.infinityNikki.initialization.notReady'),
      description: t('settings.extensions.infinityNikki.initialization.notReadyDescription'),
    }
  }

  return {
    badgeVariant: 'default',
    badgeLabel: t('settings.extensions.infinityNikki.initialization.pending'),
    description: t('settings.extensions.infinityNikki.initialization.pendingDescription'),
  }
})

function buildInfinityNikkiExePath(dir: string): string {
  const base = normalizeDirectoryPath(dir).replace(/\\/g, '/')
  return `${base}/InfinityNikki.exe`
}

function normalizeDirectoryPath(dir: string): string {
  return dir.trim().replace(/[\/]+$/, '')
}

async function isValidInfinityNikkiGameDir(dir: string): Promise<boolean> {
  const normalizedDir = normalizeDirectoryPath(dir)
  if (!normalizedDir) {
    return false
  }

  const dirInfo = await getFileInfo(normalizedDir)
  if (!dirInfo.exists || !dirInfo.isDirectory) {
    return false
  }

  const exeInfo = await getFileInfo(buildInfinityNikkiExePath(normalizedDir))
  return exeInfo.exists && exeInfo.isRegularFile
}

function getErrorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error)
}

const handleClearError = () => {
  clearError()
}

const handleResetSettings = async () => {
  await resetExtensionSettings()
}

const handleEnableChange = async (enabled: boolean) => {
  try {
    await updateInfinityNikkiEnabled(enabled)
  } catch (error) {
    toast.error(t('settings.extensions.infinityNikki.saveFailedTitle'), {
      description: getErrorMessage(error),
    })
  }
}

const handleAllowOnlinePhotoMetadataExtractChange = async (enabled: boolean) => {
  try {
    await updateInfinityNikkiAllowOnlinePhotoMetadataExtract(enabled)
  } catch (error) {
    toast.error(t('settings.extensions.infinityNikki.saveFailedTitle'), {
      description: getErrorMessage(error),
    })
  }
}

const handleManageScreenshotHardlinksChange = async (enabled: boolean) => {
  try {
    await updateInfinityNikkiManageScreenshotHardlinks(enabled)
  } catch (error) {
    toast.error(t('settings.extensions.infinityNikki.saveFailedTitle'), {
      description: getErrorMessage(error),
    })
  }
}

const handleDetectGameDir = async () => {
  if (isDetectingGameDir.value) {
    return
  }

  isDetectingGameDir.value = true
  try {
    const result = await detectInfinityNikkiGameDirectory()
    if (result.gameDirFound && result.gameDir) {
      const gameDir = normalizeDirectoryPath(result.gameDir)
      await updateInfinityNikkiGameDir(gameDir)
      toast.success(t('settings.extensions.infinityNikki.gameDir.detectSuccessTitle'), {
        description: gameDir,
      })
      return
    }

    toast.error(t('settings.extensions.infinityNikki.gameDir.detectFailedTitle'), {
      description:
        result.message || t('settings.extensions.infinityNikki.gameDir.detectFailedDescription'),
    })
  } catch (error) {
    toast.error(t('settings.extensions.infinityNikki.gameDir.detectFailedTitle'), {
      description: getErrorMessage(error),
    })
  } finally {
    isDetectingGameDir.value = false
  }
}

const handleSelectGameDir = async () => {
  if (isSelectingGameDir.value) {
    return
  }

  isSelectingGameDir.value = true
  try {
    const selectedPath = await selectDirectory(
      t('settings.extensions.infinityNikki.gameDir.dialogTitle')
    )
    if (!selectedPath) {
      return
    }

    const normalizedPath = normalizeDirectoryPath(selectedPath)
    const isValid = await isValidInfinityNikkiGameDir(normalizedPath)
    if (!isValid) {
      toast.error(t('settings.extensions.infinityNikki.gameDir.invalidTitle'), {
        description: t('settings.extensions.infinityNikki.gameDir.invalidDescription'),
      })
      return
    }

    await updateInfinityNikkiGameDir(normalizedPath)
    toast.success(t('settings.extensions.infinityNikki.gameDir.selectSuccessTitle'), {
      description: normalizedPath,
    })
  } catch (error) {
    toast.error(t('settings.extensions.infinityNikki.gameDir.selectFailedTitle'), {
      description: getErrorMessage(error),
    })
  } finally {
    isSelectingGameDir.value = false
  }
}

const handleCompleteInitialization = async () => {
  if (isCompletingInitialization.value) {
    return
  }

  if (!canCompleteInitialization.value) {
    toast.error(t('settings.extensions.infinityNikki.initialization.requirementsTitle'), {
      description: t('settings.extensions.infinityNikki.initialization.requirementsDescription'),
    })
    return
  }

  isCompletingInitialization.value = true
  try {
    await completeInfinityNikkiInitialization()
    toast.success(t('settings.extensions.infinityNikki.initialization.completeSuccessTitle'), {
      description: t('settings.extensions.infinityNikki.initialization.completeSuccessDescription'),
    })
  } catch (error) {
    toast.error(t('settings.extensions.infinityNikki.initialization.completeFailedTitle'), {
      description: getErrorMessage(error),
    })
  } finally {
    isCompletingInitialization.value = false
  }
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.extensions.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.extensions.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="handleClearError" class="mt-2">
        {{ t('settings.extensions.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.extensions.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.extensions.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.extensions.reset.title')"
        :description="t('settings.extensions.reset.description')"
        @reset="handleResetSettings"
      />
    </div>

    <div class="space-y-8">
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.extensions.infinityNikki.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.extensions.infinityNikki.description') }}
          </p>
        </div>

        <ItemGroup>
          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>{{ t('settings.extensions.infinityNikki.enable.label') }}</ItemTitle>
              <ItemDescription>
                {{ t('settings.extensions.infinityNikki.enable.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Switch
                :model-value="infinityNikkiSettings.enable"
                @update:model-value="(value) => handleEnableChange(Boolean(value))"
              />
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>{{ t('settings.extensions.infinityNikki.gameDir.label') }}</ItemTitle>
              <ItemDescription class="line-clamp-none font-mono text-xs break-all">
                {{ trimmedGameDir || t('settings.extensions.infinityNikki.gameDir.empty') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <div class="flex flex-col gap-2 sm:flex-row">
                <Button
                  variant="outline"
                  size="sm"
                  :disabled="isDetectingGameDir || isSelectingGameDir"
                  @click="handleDetectGameDir"
                >
                  {{
                    isDetectingGameDir
                      ? t('settings.extensions.infinityNikki.gameDir.detecting')
                      : t('settings.extensions.infinityNikki.gameDir.detectButton')
                  }}
                </Button>
                <Button
                  size="sm"
                  :disabled="isSelectingGameDir || isDetectingGameDir"
                  @click="handleSelectGameDir"
                >
                  {{
                    isSelectingGameDir
                      ? t('settings.extensions.infinityNikki.gameDir.selecting')
                      : t('settings.extensions.infinityNikki.gameDir.selectButton')
                  }}
                </Button>
              </div>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <div class="flex items-center gap-2">
                <ItemTitle>
                  {{ t('settings.extensions.infinityNikki.initialization.label') }}
                </ItemTitle>
                <Badge :variant="initializationStatus.badgeVariant">
                  {{ initializationStatus.badgeLabel }}
                </Badge>
              </div>
              <ItemDescription>
                {{ initializationStatus.description }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Button
                v-if="!infinityNikkiSettings.galleryGuideSeen"
                size="sm"
                :disabled="isCompletingInitialization || !canCompleteInitialization"
                @click="handleCompleteInitialization"
              >
                {{
                  isCompletingInitialization
                    ? t('settings.extensions.infinityNikki.initialization.completing')
                    : t('settings.extensions.infinityNikki.initialization.completeButton')
                }}
              </Button>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>{{ t('settings.extensions.infinityNikki.metadata.label') }}</ItemTitle>
              <ItemDescription>
                {{ t('settings.extensions.infinityNikki.metadata.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Switch
                :model-value="infinityNikkiSettings.allowOnlinePhotoMetadataExtract"
                @update:model-value="
                  (value) => handleAllowOnlinePhotoMetadataExtractChange(Boolean(value))
                "
              />
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>{{ t('settings.extensions.infinityNikki.hardlinks.label') }}</ItemTitle>
              <ItemDescription>
                {{ t('settings.extensions.infinityNikki.hardlinks.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Switch
                :model-value="infinityNikkiSettings.manageScreenshotHardlinks"
                @update:model-value="
                  (value) => handleManageScreenshotHardlinksChange(Boolean(value))
                "
              />
            </ItemActions>
          </Item>
        </ItemGroup>
      </div>
    </div>
  </div>
</template>
