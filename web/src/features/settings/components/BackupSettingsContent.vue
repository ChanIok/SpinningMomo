<script setup lang="ts">
import { ref } from 'vue'
import { Download, LoaderCircle, Upload } from 'lucide-vue-next'
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
import { Button } from '@/components/ui/button'
import {
  Item,
  ItemActions,
  ItemContent,
  ItemDescription,
  ItemTitle,
  ItemGroup,
} from '@/components/ui/item'
import { useI18n } from '@/composables/useI18n'
import { useToast } from '@/composables/useToast'
import { exportBackup, restoreBackup, selectBackupArchive } from '../backupApi'
import { selectDirectory } from '../api'

const { t } = useI18n()
const { toast } = useToast()
const isExporting = ref(false)
const isRestoring = ref(false)
const restoreDialogOpen = ref(false)
const selectedBackupPath = ref('')

// 选择目标目录后执行完整导出，RPC 不设超时以容纳大数据库和背景文件。
const handleExport = async () => {
  try {
    const destinationDirectory = await selectDirectory(t('settings.backup.export.dialogTitle'))
    if (!destinationDirectory) return

    isExporting.value = true
    const result = await exportBackup(destinationDirectory)
    toast.success(t('settings.backup.export.successTitle'), {
      description: t('settings.backup.export.successDescription', { path: result.backupPath }),
    })
  } catch (error) {
    toast.error(t('settings.backup.export.failedTitle'), {
      description: error instanceof Error ? error.message : String(error),
    })
  } finally {
    isExporting.value = false
  }
}

// 选择 ZIP 后只进行用户确认，实际替换由应用退出后的 PowerShell 脚本完成。
const handleSelectRestore = async () => {
  try {
    const backupPath = await selectBackupArchive(t('settings.backup.restore.dialogTitle'))
    if (!backupPath) return

    selectedBackupPath.value = backupPath
    restoreDialogOpen.value = true
  } catch (error) {
    toast.error(t('settings.backup.restore.failedTitle'), {
      description: error instanceof Error ? error.message : String(error),
    })
  }
}

// 用户二次确认后启动恢复脚本，后端会在响应返回后退出并重启应用。
const handleConfirmRestore = async () => {
  if (!selectedBackupPath.value) return

  isRestoring.value = true
  try {
    await restoreBackup(selectedBackupPath.value)
    toast.info(t('settings.backup.restore.restartingTitle'), {
      description: t('settings.backup.restore.restartingDescription'),
    })
  } catch (error) {
    isRestoring.value = false
    toast.error(t('settings.backup.restore.failedTitle'), {
      description: error instanceof Error ? error.message : String(error),
    })
  }
}
</script>

<template>
  <div class="w-full">
    <div class="mb-6 flex items-center justify-between">
      <div>
        <h1 class="text-2xl font-bold text-foreground">{{ t('settings.backup.title') }}</h1>
        <p class="mt-1 text-muted-foreground">{{ t('settings.backup.description') }}</p>
      </div>
    </div>

    <div class="space-y-8">
      <div class="space-y-4">
        <ItemGroup>
          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>{{ t('settings.backup.export.title') }}</ItemTitle>
              <ItemDescription class="line-clamp-none">
                {{ t('settings.backup.export.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Button variant="outline" :disabled="isExporting" @click="handleExport">
                <LoaderCircle v-if="isExporting" class="mr-2 h-4 w-4 animate-spin" />
                <Download v-else class="mr-2 h-4 w-4" />
                {{
                  isExporting
                    ? t('settings.backup.export.exporting')
                    : t('settings.backup.export.action')
                }}
              </Button>
            </ItemActions>
          </Item>

          <Item variant="surface" size="sm">
            <ItemContent>
              <ItemTitle>{{ t('settings.backup.restore.title') }}</ItemTitle>
              <ItemDescription class="line-clamp-none">
                {{ t('settings.backup.restore.description') }}
              </ItemDescription>
            </ItemContent>
            <ItemActions>
              <Button variant="destructive" :disabled="isExporting" @click="handleSelectRestore">
                <Upload class="mr-2 h-4 w-4" />
                {{ t('settings.backup.restore.action') }}
              </Button>
            </ItemActions>
          </Item>
        </ItemGroup>

        <p class="px-1 text-xs leading-5 text-muted-foreground">
          {{ t('settings.backup.scopeNotice') }}
        </p>
      </div>
    </div>

    <AlertDialog :open="restoreDialogOpen" @update:open="restoreDialogOpen = $event">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>{{ t('settings.backup.restore.confirmTitle') }}</AlertDialogTitle>
          <AlertDialogDescription class="space-y-3">
            <span class="block">{{ t('settings.backup.restore.confirmDescription') }}</span>
          </AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel :disabled="isRestoring">
            {{ t('settings.backup.restore.cancel') }}
          </AlertDialogCancel>
          <AlertDialogAction
            :disabled="isRestoring"
            class="bg-destructive text-destructive-foreground hover:bg-destructive/90"
            @click.prevent="handleConfirmRestore"
          >
            <LoaderCircle v-if="isRestoring" class="mr-2 h-4 w-4 animate-spin" />
            {{
              isRestoring
                ? t('settings.backup.restore.scheduling')
                : t('settings.backup.restore.confirm')
            }}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  </div>
</template>
