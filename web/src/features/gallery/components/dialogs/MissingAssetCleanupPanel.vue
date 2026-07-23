<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import {
  AlertTriangle,
  ChevronDown,
  ChevronRight,
  Clipboard,
  Folder,
  Trash2,
} from 'lucide-vue-next'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from '@/components/ui/alert-dialog'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { copyToClipboard, formatFileSize } from '@/lib/utils'
import { galleryApi } from '../../api'
import type { MissingAssetItem, MissingAssetsResponse } from '../../types'

const { t } = useI18n()
const { toast } = useToast()

const data = ref<MissingAssetsResponse | null>(null)
const confirmAllOpen = ref(false)
const expandedFolders = ref<Set<string>>(new Set())

const groups = computed(() => {
  const grouped = new Map<string, MissingAssetItem[]>()
  for (const item of data.value?.items ?? []) {
    const folder = getParentPath(item.path)
    const items = grouped.get(folder) ?? []
    items.push(item)
    grouped.set(folder, items)
  }
  return [...grouped.entries()].map(([path, items]) => ({ path, items }))
})

function getParentPath(path: string): string {
  const slashIndex = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'))
  return slashIndex >= 0 ? path.slice(0, slashIndex + 1) : path
}

function truncateMiddle(path: string, maxLength = 64): string {
  if (path.length <= maxLength) return path
  const sideLength = Math.floor((maxLength - 1) / 2)
  return `${path.slice(0, sideLength)}…${path.slice(-sideLength)}`
}

function formatCleanupSize(bytes?: number): string {
  return bytes && bytes > 0 ? formatFileSize(bytes) : '0 B'
}

function isExpanded(path: string): boolean {
  return expandedFolders.value.has(path)
}

function toggleFolder(path: string) {
  const next = new Set(expandedFolders.value)
  if (next.has(path)) next.delete(path)
  else next.add(path)
  expandedFolders.value = next
}

async function load() {
  try {
    data.value = await galleryApi.getMissingAssets()
    expandedFolders.value = new Set(groups.value.map((group) => group.path))
  } catch (cause) {
    console.error('Failed to load missing assets:', cause)
    toast.error(t('gallery.preferences.maintenance.loadFailed'))
  }
}

async function copyPath(path: string) {
  const copied = await copyToClipboard(path)
  if (copied) {
    toast.success(t('gallery.preferences.maintenance.copySuccess'))
  } else {
    toast.error(t('gallery.preferences.maintenance.copyFailed'))
  }
}

async function purgeOne(assetId: number) {
  try {
    const result = await galleryApi.purgeMissingAssets({ ids: [assetId] })
    if (result.deletedAssetCount === 0) {
      toast.info(t('gallery.preferences.maintenance.alreadyRestored'))
    } else {
      toast.success(t('gallery.preferences.maintenance.singleSuccess'))
    }
    await load()
  } catch (cause) {
    console.error('Failed to purge missing asset:', cause)
    toast.error(t('gallery.preferences.maintenance.purgeFailed'))
  }
}

async function purgeAll() {
  try {
    const result = await galleryApi.purgeMissingAssets()
    toast.success(
      t('gallery.preferences.maintenance.allSuccess', {
        count: result.deletedAssetCount,
        size: formatCleanupSize(result.releasedThumbnailBytes),
      })
    )
    confirmAllOpen.value = false
    await load()
  } catch (cause) {
    console.error('Failed to purge all missing assets:', cause)
    toast.error(t('gallery.preferences.maintenance.purgeFailed'))
  }
}

onMounted(load)
</script>

<template>
  <div class="flex min-h-0 flex-1 flex-col gap-5">
    <div>
      <h3 class="text-base font-semibold text-foreground">
        {{ t('gallery.preferences.maintenance.title') }}
      </h3>
      <p class="mt-1 text-sm text-muted-foreground">
        {{ t('gallery.preferences.maintenance.description') }}
      </p>
    </div>

    <div class="rounded-lg border border-border/60 bg-muted/30 p-3.5">
      <div class="flex items-start justify-between gap-4">
        <div class="flex min-w-0 gap-3">
          <div class="mt-0.5 rounded-md bg-muted p-1.5 text-muted-foreground">
            <AlertTriangle class="h-4 w-4 shrink-0 text-foreground" />
          </div>
          <div class="min-w-0">
            <p class="text-sm font-medium text-foreground">
              {{
                t('gallery.preferences.maintenance.summary', {
                  count: data?.totalCount ?? 0,
                })
              }}
            </p>
            <p class="mt-0.5 text-xs text-muted-foreground">
              {{
                t('gallery.preferences.maintenance.reclaimable', {
                  size: formatCleanupSize(data?.reclaimableThumbnailBytes),
                  count: data?.reclaimableThumbnailCount ?? 0,
                })
              }}
            </p>
            <p class="mt-0.5 text-xs text-muted-foreground/75">
              {{ t('gallery.preferences.maintenance.retentionNotice') }}
            </p>
          </div>
        </div>
        <Button
          variant="destructive"
          size="sm"
          class="shrink-0"
          :disabled="!data?.totalCount"
          @click="confirmAllOpen = true"
        >
          <Trash2 class="h-3.5 w-3.5" />
          {{ t('gallery.preferences.maintenance.purgeAll') }}
        </Button>
      </div>
    </div>

    <div
      v-if="!data?.totalCount"
      class="flex flex-1 flex-col items-center justify-center text-center text-muted-foreground"
    >
      <Trash2 class="mb-2 h-8 w-8 opacity-40" />
      <p class="text-sm font-medium text-foreground">
        {{ t('gallery.preferences.maintenance.emptyTitle') }}
      </p>
      <p class="mt-1 text-xs text-muted-foreground">
        {{ t('gallery.preferences.maintenance.emptyDescription') }}
      </p>
    </div>

    <ScrollArea v-else class="min-h-0 flex-1 rounded-lg border border-border/50">
      <div class="space-y-1 p-2">
        <section v-for="group in groups" :key="group.path" class="overflow-hidden rounded-md">
          <button
            type="button"
            class="flex w-full items-center gap-2 rounded-md px-2 py-2 text-left hover:bg-muted/60"
            @click="toggleFolder(group.path)"
          >
            <ChevronDown v-if="isExpanded(group.path)" class="h-4 w-4 shrink-0" />
            <ChevronRight v-else class="h-4 w-4 shrink-0" />
            <Folder class="h-4 w-4 shrink-0 text-primary" />
            <span class="min-w-0 flex-1 truncate text-sm font-medium" :title="group.path">
              {{ truncateMiddle(group.path) }}
            </span>
            <span class="shrink-0 text-xs text-muted-foreground">
              {{ t('gallery.preferences.maintenance.folderCount', { count: group.items.length }) }}
            </span>
          </button>

          <div v-if="isExpanded(group.path)" class="ml-8 border-l pl-2">
            <div
              v-for="item in group.items"
              :key="item.id"
              class="group flex items-center gap-2 rounded-md px-2 py-2 hover:bg-muted/45"
            >
              <div class="min-w-0 flex-1">
                <p class="truncate text-sm font-medium">{{ item.name }}</p>
                <p class="truncate text-xs text-muted-foreground">
                  {{ truncateMiddle(getParentPath(item.path), 72) }}
                </p>
              </div>
              <Button
                variant="sidebarGhost"
                size="icon-xs"
                :title="t('gallery.preferences.maintenance.copyPath')"
                @click="copyPath(item.path)"
              >
                <Clipboard class="h-3.5 w-3.5" />
              </Button>
              <Button
                variant="sidebarGhost"
                size="icon-xs"
                class="text-destructive hover:text-destructive"
                :title="t('gallery.preferences.maintenance.purgeOne')"
                @click="purgeOne(item.id)"
              >
                <Trash2 class="h-3.5 w-3.5" />
              </Button>
            </div>
          </div>
        </section>
      </div>
    </ScrollArea>

    <AlertDialog v-model:open="confirmAllOpen">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>
            {{ t('gallery.preferences.maintenance.confirmTitle') }}
          </AlertDialogTitle>
          <AlertDialogDescription>
            {{
              t('gallery.preferences.maintenance.confirmDescription', {
                count: data?.totalCount ?? 0,
              })
            }}
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>
            {{ t('gallery.preferences.maintenance.cancel') }}
          </AlertDialogCancel>
          <AlertDialogAction
            class="bg-destructive text-destructive-foreground hover:bg-destructive/90"
            @click.prevent="purgeAll"
          >
            {{ t('gallery.preferences.maintenance.confirm') }}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  </div>
</template>
