<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { Database, Palette, Trash2 } from '@lucide/vue'
import { Dialog, DialogContent, DialogDescription, DialogTitle } from '@/components/ui/dialog'
import { Switch } from '@/components/ui/switch'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Item, ItemActions, ItemContent, ItemDescription, ItemTitle } from '@/components/ui/item'
import { useI18n } from '@/composables/useI18n'
import { useGalleryStore } from '../../store'
import { useSettingsStore } from '@/features/settings/store'
import type { GalleryDeleteMode } from '../../store/persistence'
import MissingAssetCleanupPanel from './MissingAssetCleanupPanel.vue'

type PreferencesTab = 'view' | 'deletion' | 'maintenance'

const props = defineProps<{ open: boolean }>()
const emit = defineEmits<{ 'update:open': [value: boolean] }>()

const { t } = useI18n()
const store = useGalleryStore()
const settingsStore = useSettingsStore()
const activeTab = ref<PreferencesTab>('view')

const isInfinityNikkiEnabled = computed(
  () => settingsStore.appSettings.extensions.infinityNikki.enable
)

const useOriginalImagesForCards = computed({
  get: () => store.view.useOriginalImagesForCards,
  set: (value: boolean) => {
    store.view.useOriginalImagesForCards = value
  },
})

const showRatingBadge = computed({
  get: () => store.view.showRatingBadge,
  set: (value: boolean) => {
    store.view.showRatingBadge = value
  },
})

const showDyeCodeBadge = computed({
  get: () => store.view.showDyeCodeBadge,
  set: (value: boolean) => {
    store.view.showDyeCodeBadge = value
  },
})

const showTagBadges = computed({
  get: () => store.view.showTagBadges,
  set: (value: boolean) => {
    store.view.showTagBadges = value
  },
})

const deleteMode = computed({
  get: () => store.gallerySettings.deletion.mode,
  set: (value: GalleryDeleteMode) => {
    store.gallerySettings.deletion.mode = value
  },
})

const confirmRecycleBin = computed({
  get: () => store.gallerySettings.deletion.confirmRecycleBin,
  set: (value: boolean) => {
    store.gallerySettings.deletion.confirmRecycleBin = value
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
            activeTab === 'deletion'
              ? 'bg-accent text-accent-foreground'
              : 'text-muted-foreground hover:bg-muted/50 hover:text-foreground'
          "
          @click="activeTab = 'deletion'"
        >
          <Trash2 class="h-4 w-4 shrink-0" />
          {{ t('gallery.preferences.tabs.deletion') }}
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

          <div class="space-y-3">
            <h4 class="text-sm font-medium text-foreground">
              {{ t('gallery.preferences.view.image.title') }}
            </h4>
            <Item variant="surface" size="sm">
              <ItemContent>
                <ItemTitle>
                  {{ t('gallery.preferences.view.originalImages.title') }}
                </ItemTitle>
                <ItemDescription>
                  {{ t('gallery.preferences.view.originalImages.description') }}
                </ItemDescription>
              </ItemContent>
              <ItemActions>
                <Switch v-model="useOriginalImagesForCards" />
              </ItemActions>
            </Item>
          </div>

          <div class="space-y-3">
            <h4 class="text-sm font-medium text-foreground">
              {{ t('gallery.preferences.view.markers.title') }}
            </h4>
            <div class="space-y-2">
              <Item variant="surface" size="sm">
                <ItemContent>
                  <ItemTitle>
                    {{ t('gallery.preferences.view.rating.title') }}
                  </ItemTitle>
                  <ItemDescription>
                    {{ t('gallery.preferences.view.rating.description') }}
                  </ItemDescription>
                </ItemContent>
                <ItemActions>
                  <Switch v-model="showRatingBadge" />
                </ItemActions>
              </Item>

              <Item v-if="isInfinityNikkiEnabled" variant="surface" size="sm">
                <ItemContent>
                  <ItemTitle>
                    {{ t('gallery.preferences.view.dyeCode.title') }}
                  </ItemTitle>
                  <ItemDescription>
                    {{ t('gallery.preferences.view.dyeCode.description') }}
                  </ItemDescription>
                </ItemContent>
                <ItemActions>
                  <Switch v-model="showDyeCodeBadge" />
                </ItemActions>
              </Item>

              <Item variant="surface" size="sm">
                <ItemContent>
                  <ItemTitle>
                    {{ t('gallery.preferences.view.tags.title') }}
                  </ItemTitle>
                  <ItemDescription>
                    {{ t('gallery.preferences.view.tags.description') }}
                  </ItemDescription>
                </ItemContent>
                <ItemActions>
                  <Switch v-model="showTagBadges" />
                </ItemActions>
              </Item>
            </div>
          </div>
        </div>

        <div v-else-if="activeTab === 'deletion'" class="space-y-6">
          <div>
            <h3 class="text-base font-semibold text-foreground">
              {{ t('gallery.preferences.deletion.title') }}
            </h3>
            <p class="mt-1 text-sm text-muted-foreground">
              {{ t('gallery.preferences.deletion.description') }}
            </p>
          </div>

          <div class="space-y-2">
            <Item variant="surface" size="sm">
              <ItemContent>
                <ItemTitle>
                  {{ t('gallery.preferences.deletion.mode.title') }}
                </ItemTitle>
                <ItemDescription>
                  {{ t('gallery.preferences.deletion.mode.description') }}
                </ItemDescription>
              </ItemContent>
              <ItemActions>
                <Select v-model="deleteMode">
                  <SelectTrigger class="w-36">
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent>
                    <SelectItem value="recycleBin">
                      {{ t('gallery.preferences.deletion.mode.recycleBin') }}
                    </SelectItem>
                    <SelectItem value="permanent">
                      {{ t('gallery.preferences.deletion.mode.permanent') }}
                    </SelectItem>
                  </SelectContent>
                </Select>
              </ItemActions>
            </Item>

            <Item v-if="deleteMode === 'recycleBin'" variant="surface" size="sm">
              <ItemContent>
                <ItemTitle>
                  {{ t('gallery.preferences.deletion.confirm.title') }}
                </ItemTitle>
                <ItemDescription>
                  {{ t('gallery.preferences.deletion.confirm.description') }}
                </ItemDescription>
              </ItemContent>
              <ItemActions>
                <Switch v-model="confirmRecycleBin" />
              </ItemActions>
            </Item>
          </div>

          <p
            class="rounded-md border border-border/40 bg-muted/30 px-3.5 py-3 text-xs text-muted-foreground"
          >
            {{ t('gallery.preferences.deletion.permanentNotice') }}
          </p>
        </div>

        <MissingAssetCleanupPanel v-else />
      </main>
    </DialogContent>
  </Dialog>
</template>
