import { useState } from 'react'
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
import { RotateCcw } from 'lucide-react'
import type { ReactNode } from 'react'
import { useTranslation } from '@/lib/i18n'

interface ResetSettingsDialogProps {
  title: string
  description: string
  onReset: () => Promise<void>
  trigger?: ReactNode
  triggerText?: string
  confirmText?: string
  cancelText?: string
}

export function ResetSettingsDialog({
  title,
  description,
  onReset,
  trigger,
  triggerText,
  confirmText,
  cancelText,
}: ResetSettingsDialogProps) {
  const { t } = useTranslation()
  const [isResetting, setIsResetting] = useState(false)

  const handleReset = async () => {
    setIsResetting(true)
    try {
      await onReset()
    } finally {
      setIsResetting(false)
    }
  }

  return (
    <AlertDialog>
      <AlertDialogTrigger asChild>
        {trigger || (
          <Button
            variant='outline'
            size='sm'
            className='shrink-0 bg-background/80 dark:bg-secondary/80 dark:hover:bg-secondary'
          >
            <RotateCcw className='mr-2 h-4 w-4' />
            {triggerText || t('settings.reset.dialog.triggerText')}
          </Button>
        )}
      </AlertDialogTrigger>
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>{title}</AlertDialogTitle>
          <AlertDialogDescription>{description}</AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>
            {cancelText || t('settings.reset.dialog.cancelText')}
          </AlertDialogCancel>
          <AlertDialogAction onClick={handleReset} disabled={isResetting}>
            {isResetting
              ? t('settings.reset.dialog.resetting')
              : confirmText || t('settings.reset.dialog.confirmText')}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  )
}
