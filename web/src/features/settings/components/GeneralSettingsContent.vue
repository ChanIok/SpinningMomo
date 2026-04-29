<script setup lang="ts">
import { useSettingsStore } from '../store'
import { useGeneralActions } from '../composables/useGeneralActions'
import { storeToRefs } from 'pinia'
import { RouterLink } from 'vue-router'
import { Switch } from '@/components/ui/switch'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Button } from '@/components/ui/button'
import { Item, ItemContent, ItemTitle, ItemDescription, ItemActions } from '@/components/ui/item'
import ResetSettingsDialog from './ResetSettingsDialog.vue'
import { useI18n } from '@/composables/useI18n'

const store = useSettingsStore()
const { appSettings, error, isInitialized } = storeToRefs(store)
const {
  updateLanguage,
  updateLoggerLevel,
  updateAutoCheck,
  updateAutoUpdateOnExit,
  updateDownloadSourceMode,
  resetGeneralCoreSettings,
} = useGeneralActions()
const { clearError } = store
const { t } = useI18n()

type UpdateSourceMode = 'cn_first' | 'github_first'

const getCurrentUpdateSourceMode = (): UpdateSourceMode => {
  const order = appSettings.value.update.downloadSources.map((item) => item.name)
  if (order.join(',') === 'CNB,Mirror,GitHub') {
    return 'cn_first'
  }
  return 'github_first'
}

const handleReset = async () => {
  await resetGeneralCoreSettings()
  // simple visual feedback handled by dialog
}
</script>

<template>
  <div v-if="!isInitialized" class="flex items-center justify-center p-6">
    <div class="text-center">
      <div
        class="mx-auto h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary"
      ></div>
      <p class="mt-2 text-sm text-muted-foreground">{{ t('settings.loading') }}</p>
    </div>
  </div>

  <div v-else-if="error" class="flex items-center justify-center p-6">
    <div class="text-center">
      <p class="text-sm text-muted-foreground">{{ t('settings.error.title') }}</p>
      <p class="mt-1 text-sm text-red-500">{{ error }}</p>
      <Button variant="outline" size="sm" @click="clearError" class="mt-2">
        {{ t('settings.error.retry') }}
      </Button>
    </div>
  </div>

  <div v-else class="w-full">
    <!-- Header -->
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.general.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.general.description') }}</p>
      </div>
      <ResetSettingsDialog
        :title="t('settings.reset.title')"
        :description="t('settings.reset.description')"
        @reset="handleReset"
      />
    </div>

    <div class="space-y-8">
      <!-- Language -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.general.language.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.general.language.description') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('common.languageLabelBilingual') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.general.language.displayLanguageDescription') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings.app.language.current"
              @update:model-value="(v) => updateLanguage(v as string)"
            >
              <SelectTrigger class="w-48">
                <SelectValue :placeholder="t('common.languageLabelBilingual')" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="zh-CN">{{ t('common.languageZhCn') }}</SelectItem>
                <SelectItem value="en-US">{{ t('common.languageEnUs') }}</SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>
      </div>

      <!-- Logger -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.general.logger.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.general.logger.description') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.general.logger.level') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.general.logger.levelDescription') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="appSettings.app.logger.level"
              @update:model-value="(v) => updateLoggerLevel(v as string)"
            >
              <SelectTrigger class="w-48">
                <SelectValue :placeholder="t('settings.general.logger.level')" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="DEBUG">DEBUG</SelectItem>
                <SelectItem value="INFO">INFO</SelectItem>
                <SelectItem value="ERROR">ERROR</SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>
      </div>

      <!-- Update -->
      <div class="space-y-4">
        <div>
          <h3 class="text-lg font-semibold text-foreground">
            {{ t('settings.general.update.title') }}
          </h3>
          <p class="mt-1 text-sm text-muted-foreground">
            {{ t('settings.general.update.descriptionPrefix') }}
            <RouterLink
              :to="{ name: 'about' }"
              class="font-medium text-primary underline underline-offset-4 hover:text-primary/80"
            >
              {{ t('settings.general.update.descriptionLink') }}
            </RouterLink>
            {{ t('settings.general.update.descriptionSuffix') }}
          </p>
        </div>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.general.update.sourceMode.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.general.update.sourceMode.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Select
              :model-value="getCurrentUpdateSourceMode()"
              @update:model-value="(v) => updateDownloadSourceMode(v as UpdateSourceMode)"
            >
              <SelectTrigger class="w-56">
                <SelectValue :placeholder="t('settings.general.update.sourceMode.label')" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="cn_first">
                  {{ t('settings.general.update.sourceMode.cnFirst') }}
                </SelectItem>
                <SelectItem value="github_first">
                  {{ t('settings.general.update.sourceMode.githubFirst') }}
                </SelectItem>
              </SelectContent>
            </Select>
          </ItemActions>
        </Item>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.general.update.autoCheck.label') }}
            </ItemTitle>
            <ItemDescription>
              {{ t('settings.general.update.autoCheck.description') }}
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Switch
              :model-value="appSettings.update.autoCheck"
              @update:model-value="(value) => updateAutoCheck(Boolean(value))"
            />
          </ItemActions>
        </Item>

        <Item variant="surface" size="sm">
          <ItemContent>
            <ItemTitle>
              {{ t('settings.general.update.autoUpdateOnExit.label') }}
            </ItemTitle>
            <ItemDescription class="line-clamp-none">
              {{ t('settings.general.update.autoUpdateOnExit.description') }}
              <span v-if="!appSettings.update.autoCheck" class="mt-1 block text-xs">
                {{ t('settings.general.update.autoUpdateOnExit.requiresAutoCheck') }}
              </span>
            </ItemDescription>
          </ItemContent>
          <ItemActions>
            <Switch
              :model-value="appSettings.update.autoUpdateOnExit"
              :disabled="!appSettings.update.autoCheck"
              @update:model-value="(value) => updateAutoUpdateOnExit(Boolean(value))"
            />
          </ItemActions>
        </Item>
      </div>
    </div>
  </div>
</template>
