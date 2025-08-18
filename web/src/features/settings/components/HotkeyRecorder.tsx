import { useState, useEffect, useRef } from 'react'
import { cn } from '@/lib/utils'
import { 
  formatHotkeyDisplay, 
  calculateModifiers 
} from '../utils/hotkey-utils'

interface Hotkey {
  modifiers: number
  key: number
}

interface HotkeyRecorderProps {
  value: Hotkey
  onChange: (hotkey: Hotkey) => void
  className?: string
}

export function HotkeyRecorder({ value, onChange, className }: HotkeyRecorderProps) {
  const [isRecording, setIsRecording] = useState(false)
  const [displayText, setDisplayText] = useState('')
  const [currentModifiers, setCurrentModifiers] = useState(0)
  const [currentKey, setCurrentKey] = useState(0)
  const recorderRef = useRef<HTMLDivElement>(null)

  // 更新显示文本
  useEffect(() => {
    setDisplayText(formatHotkeyDisplay(value.modifiers, value.key))
  }, [value])

  // 实时更新录制时的显示文本
  useEffect(() => {
    if (isRecording) {
      setDisplayText(formatHotkeyDisplay(currentModifiers, currentKey) || '请按下快捷键...')
    }
  }, [isRecording, currentModifiers, currentKey])

  // 开始录制
  const startRecording = () => {
    setIsRecording(true)
    setCurrentModifiers(0)
    setCurrentKey(0)
  }

  // 停止录制
  const stopRecording = () => {
    setIsRecording(false)
    setCurrentModifiers(0)
    setCurrentKey(0)
  }

  // 处理键盘事件
  useEffect(() => {
    if (!isRecording) return

    const handleKeyDown = (e: KeyboardEvent) => {
      // 阻止默认行为
      e.preventDefault()
      e.stopPropagation()

      // 处理特殊按键
      if (e.key === 'Escape') {
        // 取消录制
        stopRecording()
        return
      }

      if (e.key === 'Backspace') {
        // 清除快捷键
        onChange({ modifiers: 0, key: 0 })
        stopRecording()
        return
      }

      // 获取修饰键状态
      const modifiers = calculateModifiers(e.shiftKey, e.ctrlKey, e.altKey)
      
      // 如果是修饰键，只更新修饰键状态并实时显示
      if (['Control', 'ControlLeft', 'ControlRight', 'Alt', 'AltLeft', 'AltRight', 'Shift', 'ShiftLeft', 'ShiftRight'].includes(e.key)) {
        setCurrentModifiers(modifiers)
        return
      }

      // 获取主键
      const key = e.keyCode
      setCurrentKey(key)
      setCurrentModifiers(modifiers)

      // 更新值
      onChange({ modifiers, key })
      
      // 停止录制
      stopRecording()
    }

    const handleKeyUp = (e: KeyboardEvent) => {
      // 当修饰键被释放时更新修饰键状态
      if (['Control', 'ControlLeft', 'ControlRight', 'Alt', 'AltLeft', 'AltRight', 'Shift', 'ShiftLeft', 'ShiftRight'].includes(e.key)) {
        const modifiers = calculateModifiers(
          e.key.startsWith('Shift') ? false : (e.shiftKey || false),
          e.key.startsWith('Control') ? false : (e.ctrlKey || false),
          e.key.startsWith('Alt') ? false : (e.altKey || false)
        )
        setCurrentModifiers(modifiers)
      }
    }

    // 添加事件监听器
    window.addEventListener('keydown', handleKeyDown)
    window.addEventListener('keyup', handleKeyUp)
    
    // 清理函数
    return () => {
      window.removeEventListener('keydown', handleKeyDown)
      window.removeEventListener('keyup', handleKeyUp)
    }
  }, [isRecording, onChange])

  // 点击外部停止录制
  useEffect(() => {
    if (!isRecording) return

    const handleClickOutside = (e: MouseEvent) => {
      if (recorderRef.current && !recorderRef.current.contains(e.target as Node)) {
        stopRecording()
      }
    }

    document.addEventListener('mousedown', handleClickOutside)
    return () => {
      document.removeEventListener('mousedown', handleClickOutside)
    }
  }, [isRecording])

  return (
    <div 
      ref={recorderRef}
      className={cn('w-full', className)}
    >
      <div 
        className={cn(
          'px-3 py-2 rounded-md border border-input bg-background text-sm',
          'focus-visible:ring-ring focus-visible:ring-2 focus-visible:ring-offset-2',
          'transition-colors cursor-pointer',
          isRecording 
            ? 'border-primary ring-2 ring-primary ring-offset-2' 
            : 'hover:border-accent-foreground'
        )}
        onClick={startRecording}
        tabIndex={0}
      >
        {isRecording ? (displayText || '请按下快捷键...') : displayText}
      </div>
    </div>
  )
}