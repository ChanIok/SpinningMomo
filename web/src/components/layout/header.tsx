import { call } from '@/lib/rpc'
import { Button } from '@/components/ui/button'
import { Minus, Square, X, Minimize2 } from 'lucide-react'
import { useState, useEffect } from 'react'

import './header.css'

export function Header() {
  const [isMaximized, setIsMaximized] = useState(false)

  // 监听窗口状态变化
  useEffect(() => {
    // 这里可以添加监听窗口状态变化的逻辑
    // 暂时使用简单的方法检测最大化状态
  }, [])

  const handleMinimize = () => {
    call('webview.minimize').catch((err) => {
      console.error('Failed to minimize window:', err)
    })
  }

  const handleMaximizeToggle = () => {
    if (isMaximized) {
      call('webview.restore')
        .then(() => {
          setIsMaximized(false)
        })
        .catch((err) => {
          console.error('Failed to restore window:', err)
        })
    } else {
      call('webview.maximize')
        .then(() => {
          setIsMaximized(true)
        })
        .catch((err) => {
          console.error('Failed to maximize window:', err)
        })
    }
  }

  const handleClose = () => {
    call('webview.close').catch((err) => {
      console.error('Failed to close window:', err)
    })
  }

  return (
    <header className='flex h-12 items-center justify-between gap-2 border-b bg-background/95 px-4 backdrop-blur supports-[backdrop-filter]:bg-background/60'>
      <div className='drag-region h-full flex-1'></div> {/* 可拖动区域 */}
      <div className='flex gap-2'>
        <Button
          variant='ghost'
          size='icon'
          className='h-8 w-8'
          onClick={handleMinimize}
          title='Minimize'
        >
          <Minus className='h-4 w-4' />
        </Button>

        <Button
          variant='ghost'
          size='icon'
          className='h-8 w-8'
          onClick={handleMaximizeToggle}
          title={isMaximized ? 'Restore' : 'Maximize'}
        >
          {isMaximized ? <Minimize2 className='h-4 w-4' /> : <Square className='h-4 w-4' />}
        </Button>

        <Button
          variant='ghost'
          size='icon'
          className='hover:text-destructive-foreground h-8 w-8 hover:bg-destructive'
          onClick={handleClose}
          title='Close'
        >
          <X className='h-4 w-4' />
        </Button>
      </div>
    </header>
  )
}
