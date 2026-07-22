<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryAssetActions } from '../../composables'
import { useGalleryStore } from '../../store'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import { ScrollArea } from '@/components/ui/scroll-area'
import FolderPickerTreeItem from '../folders/FolderPickerTreeItem.vue'

const assetActions = useGalleryAssetActions()
const store = useGalleryStore()
const { t } = useI18n()

const selectedFolderId = ref<number | null>(null)
const expandedIds = ref<Set<number>>(new Set())
const isSubmitting = ref(false)

const open = computed({
  get: () => assetActions.moveToFolderDialog.open,
  set: (value: boolean) => assetActions.setMoveToFolderDialogOpen(value),
})

const folders = computed(() => store.folders)
const selectedCount = computed(() => store.selectedCount)
const canConfirm = computed(() => selectedFolderId.value !== null && !isSubmitting.value)

watch(open, () => {
  selectedFolderId.value = null
  expandedIds.value = new Set()
  isSubmitting.value = false
})

function toggleExpand(folderId: number) {
  const next = new Set(expandedIds.value)
  if (next.has(folderId)) {
    next.delete(folderId)
  } else {
    next.add(folderId)
  }
  expandedIds.value = next
}

function selectFolder(folderId: number) {
  selectedFolderId.value = folderId
}

async function handleConfirm() {
  if (selectedFolderId.value === null || isSubmitting.value) {
    return
  }

  isSubmitting.value = true
  try {
    await assetActions.handleMoveSelectedAssetsToFolder(selectedFolderId.value)
    open.value = false
  } finally {
    isSubmitting.value = false
  }
}

function handleOpenChange(value: boolean) {
  if (isSubmitting.value && !value) {
    return
  }
  open.value = value
}
</script>

<template>
  <Dialog :open="open" @update:open="handleOpenChange">
    <DialogContent class="sm:max-w-lg">
      <DialogHeader>
        <DialogTitle>{{ t('gallery.dialogs.moveToFolder.title') }}</DialogTitle>
        <DialogDescription>
          {{ t('gallery.dialogs.moveToFolder.description', { count: selectedCount }) }}
        </DialogDescription>
      </DialogHeader>

      <div class="min-h-0">
        <ScrollArea class="h-72 rounded-md border border-border">
          <div
            v-if="folders.length === 0"
            class="px-3 py-8 text-center text-sm text-muted-foreground"
          >
            {{ t('gallery.dialogs.moveToFolder.emptyTree') }}
          </div>
          <ul v-else class="space-y-0.5 p-2">
            <FolderPickerTreeItem
              v-for="folder in folders"
              :key="folder.id"
              :folder="folder"
              :depth="0"
              :selected-folder-id="selectedFolderId"
              :expanded-ids="expandedIds"
              @select="selectFolder"
              @toggle-expand="toggleExpand"
            />
          </ul>
        </ScrollArea>
      </div>

      <DialogFooter>
        <Button variant="outline" :disabled="isSubmitting" @click="open = false">
          {{ t('gallery.dialogs.moveToFolder.cancel') }}
        </Button>
        <Button :disabled="!canConfirm" @click="handleConfirm">
          {{ t('gallery.dialogs.moveToFolder.confirm') }}
        </Button>
      </DialogFooter>
    </DialogContent>
  </Dialog>
</template>
