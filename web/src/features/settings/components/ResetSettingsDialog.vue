<script setup lang="ts">
import { ref } from 'vue'
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from '@/components/ui/alert-dialog'
import { Button } from '@/components/ui/button'
import { RotateCcw } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'

interface Props {
  title: string
  description: string
  triggerText?: string
  confirmText?: string
  cancelText?: string
}

defineProps<Props>()
const emit = defineEmits<{
  (e: 'reset'): Promise<void>
}>()

const { t } = useI18n()
const isResetting = ref(false)
const open = ref(false)

const handleReset = async (e: Event) => {
  e.preventDefault() // 阻止默认关闭行为，等待异步操作
  isResetting.value = true
  try {
    await emit('reset')
    open.value = false // 手动关闭
  } finally {
    isResetting.value = false
  }
}
</script>

<template>
  <AlertDialog :open="open" @update:open="open = $event">
    <AlertDialogTrigger as-child>
      <slot name="trigger">
        <Button variant="outline" size="sm" class="surface-top shrink-0 hover:bg-accent">
          <RotateCcw class="mr-2 h-4 w-4" />
          {{ triggerText || t('settings.reset.dialog.triggerText') }}
        </Button>
      </slot>
    </AlertDialogTrigger>
    <AlertDialogContent>
      <AlertDialogHeader>
        <AlertDialogTitle>{{ title }}</AlertDialogTitle>
        <AlertDialogDescription>{{ description }}</AlertDialogDescription>
      </AlertDialogHeader>
      <AlertDialogFooter>
        <AlertDialogCancel>
          {{ cancelText || t('settings.reset.dialog.cancelText') }}
        </AlertDialogCancel>
        <!-- 使用 .prevent 防止自动关闭 -->
        <AlertDialogAction @click.prevent="handleReset" :disabled="isResetting">
          {{
            isResetting
              ? t('settings.reset.dialog.resetting')
              : confirmText || t('settings.reset.dialog.confirmText')
          }}
        </AlertDialogAction>
      </AlertDialogFooter>
    </AlertDialogContent>
  </AlertDialog>
</template>
