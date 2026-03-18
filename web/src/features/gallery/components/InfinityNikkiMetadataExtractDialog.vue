<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { startExtractInfinityNikkiPhotoParamsForFolder } from '@/extensions/infinity_nikki'
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
import { Switch } from '@/components/ui/switch'
import { Loader2 } from 'lucide-vue-next'

interface Props {
  open: boolean
  folderId: number | null
  folderName: string
}

const props = defineProps<Props>()
const emit = defineEmits<{
  'update:open': [value: boolean]
}>()

const { t } = useI18n()
const { toast } = useToast()

const uid = ref('')
const onlyMissing = ref(false)
const isSubmitting = ref(false)

const isUidValid = computed(() => /^\d+$/.test(uid.value.trim()))
const canSubmit = computed(() => props.folderId !== null && isUidValid.value && !isSubmitting.value)

function resetForm() {
  uid.value = ''
  onlyMissing.value = false
}

watch(
  () => props.open,
  (open) => {
    if (!open && !isSubmitting.value) {
      resetForm()
    }
  }
)

function handleOpenChange(open: boolean) {
  if (!open && isSubmitting.value) {
    return
  }
  emit('update:open', open)
}

async function handleSubmit() {
  if (props.folderId === null) {
    return
  }

  const trimmedUid = uid.value.trim()
  if (!/^\d+$/.test(trimmedUid)) {
    toast.error(t('gallery.infinityNikki.extractDialog.invalidUidTitle'), {
      description: t('gallery.infinityNikki.extractDialog.invalidUidDescription'),
    })
    return
  }

  isSubmitting.value = true
  const loadingToastId = toast.loading(t('gallery.infinityNikki.extractDialog.submitting'))

  try {
    const taskId = await startExtractInfinityNikkiPhotoParamsForFolder({
      folderId: props.folderId,
      uid: trimmedUid,
      onlyMissing: onlyMissing.value,
    })

    toast.dismiss(loadingToastId)
    toast.success(t('gallery.infinityNikki.extractDialog.successTitle'), {
      description: t('gallery.infinityNikki.extractDialog.successDescription', {
        taskId,
      }),
    })

    emit('update:open', false)
    resetForm()
  } catch (error) {
    toast.dismiss(loadingToastId)
    const message = error instanceof Error ? error.message : String(error)
    toast.error(t('gallery.infinityNikki.extractDialog.failedTitle'), {
      description: message,
    })
  } finally {
    isSubmitting.value = false
  }
}
</script>

<template>
  <Dialog :open="open" @update:open="handleOpenChange">
    <DialogContent class="sm:max-w-[460px]" :show-close-button="false">
      <DialogHeader>
        <DialogTitle>{{ t('gallery.infinityNikki.extractDialog.title') }}</DialogTitle>
        <DialogDescription>
          {{
            t('gallery.infinityNikki.extractDialog.description', {
              folderName,
            })
          }}
        </DialogDescription>
      </DialogHeader>

      <div class="space-y-4 py-2">
        <div class="space-y-2">
          <Label for="infinity-nikki-folder-uid">
            {{ t('gallery.infinityNikki.extractDialog.uidLabel') }}
          </Label>
          <Input
            id="infinity-nikki-folder-uid"
            v-model="uid"
            :placeholder="t('gallery.infinityNikki.extractDialog.uidPlaceholder')"
            inputmode="numeric"
            autocomplete="off"
          />
          <p class="text-xs text-muted-foreground">
            {{ t('gallery.infinityNikki.extractDialog.uidHint') }}
          </p>
        </div>

        <div class="flex items-center justify-between gap-4 rounded-md border p-3">
          <div class="space-y-1">
            <p class="text-sm font-medium text-foreground">
              {{ t('gallery.infinityNikki.extractDialog.onlyMissingLabel') }}
            </p>
            <p class="text-xs text-muted-foreground">
              {{ t('gallery.infinityNikki.extractDialog.onlyMissingHint') }}
            </p>
          </div>
          <Switch v-model:model-value="onlyMissing" :disabled="isSubmitting" />
        </div>
      </div>

      <DialogFooter>
        <Button variant="outline" :disabled="isSubmitting" @click="handleOpenChange(false)">
          {{ t('gallery.infinityNikki.extractDialog.cancel') }}
        </Button>
        <Button :disabled="!canSubmit" @click="handleSubmit">
          <Loader2 v-if="isSubmitting" class="mr-2 h-4 w-4 animate-spin" />
          {{
            isSubmitting
              ? t('gallery.infinityNikki.extractDialog.submitting')
              : t('gallery.infinityNikki.extractDialog.confirm')
          }}
        </Button>
      </DialogFooter>
    </DialogContent>
  </Dialog>
</template>
