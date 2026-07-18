import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'

export interface BackupExportResult {
  backupPath: string
  appVersion: string
  createdAt: number
  size: number
}

export async function selectBackupArchive(title: string): Promise<string | null> {
  const parentWindowMode = isWebView() ? 1 : 2
  const result = await call<{ paths: string[] }>(
    'dialog.openFile',
    {
      title,
      filter: 'ZIP backup (*.zip)|*.zip',
      allowMultiple: false,
      parentWindowMode,
    },
    0
  )
  return result.paths[0] || null
}

export async function exportBackup(destinationDirectory: string): Promise<BackupExportResult> {
  return call<BackupExportResult>('backup.export', { destinationDirectory }, 0)
}

export async function restoreBackup(backupPath: string): Promise<void> {
  await call('backup.restore', { backupPath }, 0)
}
