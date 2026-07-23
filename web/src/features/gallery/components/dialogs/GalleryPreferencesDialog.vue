<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Database, Palette } from 'lucide-vue-next'
import { Dialog, DialogContent, DialogDescription, DialogTitle } from '@/components/ui/dialog'
import { Switch } from '@/components/ui/switch'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'
import { useSettingsStore } from '@/features/settings/store'
import MissingAssetCleanupPanel from './MissingAssetCleanupPanel.vue'

type PreferencesTab = 'view' | 'maintenance'

const props = defineProps<{ open: boolean }>()
const emit = defineEmits<{ 'update:open': [value: boolean] }>()

const { t } = useI18n()
const store = useGalleryStore()
const settingsStore = useSettingsStore()
const activeTab = ref<PreferencesTab>('view')

const isInfinityNikkiEnabled = computed(
  () => settingsStore.appSettings.extensions.infinityNikki.enable
)

const showRatingBadge = computed({
  get: () => store.gallerySettings.view.showRatingBadge,
  set: (value: boolean) => {
    store.gallerySettings.view.showRatingBadge = value
  },
})

const showDyeCodeBadge = computed({
  get: () => store.gallerySettings.view.showDyeCodeBadge,
  set: (value: boolean) => {
    store.gallerySettings.view.showDyeCodeBadge = value
  },
})

watch(
  () => props.open,
  (open) => {
    if (!open) return
    activeTab.value = 'view'
  }
)
</script>

<template>
  <Dialog :open="open" @update:open="emit('update:open', $event)">
    <DialogContent
      class="flex h-[640px] max-h-[85vh] flex-row gap-0 overflow-hidden p-0 sm:max-w-3xl"
    >
      <DialogTitle class="sr-only">{{ t('gallery.preferences.title') }}</DialogTitle>
      <DialogDescription class="sr-only">{{
        t('gallery.preferences.description')
      }}</DialogDescription>

      <nav class="w-56 shrink-0 space-y-1.5 border-r border-border/40 bg-muted/15 p-5 pt-7">
        <div
          class="px-3 pb-2.5 text-xs font-semibold tracking-wider text-muted-foreground/70 uppercase"
        >
          {{ t('gallery.preferences.title') }}
        </div>
        <button
          type="button"
          class="flex w-full items-center gap-2.5 rounded-md px-3 py-2 text-sm font-medium transition-colors"
          :class="
            activeTab === 'view'
              ? 'bg-accent text-accent-foreground'
              : 'text-muted-foreground hover:bg-muted/50 hover:text-foreground'
          "
          @click="activeTab = 'view'"
        >
          <Palette class="h-4 w-4 shrink-0" />
          {{ t('gallery.preferences.tabs.view') }}
        </button>
        <button
          type="button"
          class="flex w-full items-center gap-2.5 rounded-md px-3 py-2 text-sm font-medium transition-colors"
          :class="
            activeTab === 'maintenance'
              ? 'bg-accent text-accent-foreground'
              : 'text-muted-foreground hover:bg-muted/50 hover:text-foreground'
          "
          @click="activeTab = 'maintenance'"
        >
          <Database class="h-4 w-4 shrink-0" />
          {{ t('gallery.preferences.tabs.maintenance') }}
        </button>
      </nav>

      <main class="flex min-w-0 flex-1 flex-col p-8 pt-7">
        <div v-if="activeTab === 'view'" class="space-y-6">
          <div>
            <h3 class="text-base font-semibold text-foreground">
              {{ t('gallery.preferences.view.title') }}
            </h3>
            <p class="mt-1 text-sm text-muted-foreground">
              {{ t('gallery.preferences.view.description') }}
            </p>
          </div>

          <div class="space-y-1">
            <label
              class="flex cursor-pointer items-center justify-between gap-6 rounded-lg p-3.5 transition-colors hover:bg-muted/40"
            >
              <div class="space-y-0.5">
                <span class="block text-sm font-medium text-foreground">
                  {{ t('gallery.preferences.view.rating.title') }}
                </span>
                <span class="block text-xs text-muted-foreground">
                  {{ t('gallery.preferences.view.rating.description') }}
                </span>
              </div>
              <Switch v-model="showRatingBadge" />
            </label>

            <label
              v-if="isInfinityNikkiEnabled"
              class="flex cursor-pointer items-center justify-between gap-6 rounded-lg p-3.5 transition-colors hover:bg-muted/40"
            >
              <div class="space-y-0.5">
                <span class="block text-sm font-medium text-foreground">
                  {{ t('gallery.preferences.view.dyeCode.title') }}
                </span>
                <span class="block text-xs text-muted-foreground">
                  {{ t('gallery.preferences.view.dyeCode.description') }}
                </span>
              </div>
              <Switch v-model="showDyeCodeBadge" />
            </label>
          </div>
        </div>

        <MissingAssetCleanupPanel v-else />
      </main>
    </DialogContent>
  </Dialog>
</template>
