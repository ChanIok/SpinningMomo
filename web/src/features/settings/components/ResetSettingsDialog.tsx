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
  triggerText = '重置默认',
  confirmText = '确认重置',
  cancelText = '取消',
}: ResetSettingsDialogProps) {
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
          <Button variant='outline' size='sm' className='shrink-0'>
            <RotateCcw className='mr-2 h-4 w-4' />
            {triggerText}
          </Button>
        )}
      </AlertDialogTrigger>
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>{title}</AlertDialogTitle>
          <AlertDialogDescription>{description}</AlertDialogDescription>
        </AlertDialogHeader>
        <AlertDialogFooter>
          <AlertDialogCancel>{cancelText}</AlertDialogCancel>
          <AlertDialogAction onClick={handleReset} disabled={isResetting}>
            {isResetting ? '重置中...' : confirmText}
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>
  )
}
