<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useGalleryAssetActions } from '../../composables'
import { Button } from '@/components/ui/button'
import {
  AlertDialog,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
} from '@/components/ui/alert-dialog'

const { t } = useI18n()
const assetActions = useGalleryAssetActions()
const isSubmitting = ref(false)

const state = computed(() => assetActions.deleteAssetsDialog)
const hasPermanentDeletion = computed(() => state.value.permanentCount > 0)
const hasUnknownLocation = computed(() => state.value.unknownCount > 0)
const isMixedDeletion = computed(
  () => state.value.permanentCount > 0 && state.value.recycleBinCount > 0
)

const title = computed(() => {
  if (state.value.mode === 'permanent') {
    return t('gallery.dialogs.delete.permanentTitle')
  }
  if (hasUnknownLocation.value) {
    return t('gallery.dialogs.delete.unknownTitle')
  }
  if (isMixedDeletion.value) {
    return t('gallery.dialogs.delete.mixedTitle')
  }
  if (hasPermanentDeletion.value) {
    return t('gallery.dialogs.delete.networkTitle')
  }
  return t('gallery.dialogs.delete.recycleBinTitle')
})

const description = computed(() => {
  if (state.value.mode === 'permanent') {
    return t('gallery.dialogs.delete.permanentDescription', {
      count: state.value.permanentCount,
    })
  }
  if (hasUnknownLocation.value) {
    return t('gallery.dialogs.delete.unknownDescription', {
      count: state.value.unknownCount,
    })
  }
  if (isMixedDeletion.value) {
    return t('gallery.dialogs.delete.mixedDescription', {
      permanent: state.value.permanentCount,
      recycled: state.value.recycleBinCount,
    })
  }
  if (hasPermanentDeletion.value) {
    return t('gallery.dialogs.delete.networkDescription', {
      count: state.value.permanentCount,
    })
  }
  return t('gallery.dialogs.delete.recycleBinDescription', {
    count: state.value.recycleBinCount,
  })
})

const confirmLabel = computed(() => {
  if (hasUnknownLocation.value) {
    return t('gallery.dialogs.delete.confirmUnknown')
  }
  return t(
    hasPermanentDeletion.value
      ? 'gallery.dialogs.delete.confirmPermanent'
      : 'gallery.dialogs.delete.confirmRecycleBin'
  )
})

watch(
  () => state.value.open,
  () => {
    isSubmitting.value = false
  }
)

function handleOpenChange(open: boolean) {
  if (isSubmitting.value && !open) {
    return
  }
  assetActions.setDeleteAssetsDialogOpen(open)
}

async function handleConfirm() {
  if (isSubmitting.value) {
    return
  }
  isSubmitting.value = true
  try {
    await assetActions.confirmDeleteAssets()
  } finally {
    isSubmitting.value = false
  }
}
</script>

<template>
  <AlertDialog :open="state.open" @update:open="handleOpenChange">
    <AlertDialogContent>
      <AlertDialogHeader>
        <AlertDialogTitle>{{ title }}</AlertDialogTitle>
        <AlertDialogDescription>{{ description }}</AlertDialogDescription>
      </AlertDialogHeader>
      <AlertDialogFooter>
        <Button variant="outline" :disabled="isSubmitting" @click="handleOpenChange(false)">
          {{ t('gallery.dialogs.delete.cancel') }}
        </Button>
        <Button variant="destructive" :disabled="isSubmitting" @click="handleConfirm">
          {{ confirmLabel }}
        </Button>
      </AlertDialogFooter>
    </AlertDialogContent>
  </AlertDialog>
</template>
