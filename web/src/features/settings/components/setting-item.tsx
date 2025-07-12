import { cn } from '@/lib/utils'
import { Button } from '@/components/ui/button'
import { Label } from '@/components/ui/label'
import { Switch } from '@/components/ui/switch'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select'
import { Input } from '@/components/ui/input'

interface SettingItemProps {
  title: string
  description?: string
  className?: string
  children?: React.ReactNode
}

interface ToggleSettingProps extends SettingItemProps {
  checked: boolean
  onCheckedChange: (checked: boolean) => void
}

interface SelectSettingProps extends SettingItemProps {
  value: string
  onValueChange: (value: string) => void
  options: { value: string; label: string }[]
  placeholder?: string
}

interface InputSettingProps extends SettingItemProps {
  value: string | number
  onChange: (value: string) => void
  type?: 'text' | 'number'
  placeholder?: string
}

interface NumberSettingProps extends SettingItemProps {
  value: number
  onValueChange: (value: number) => void
  min?: number
  max?: number
  placeholder?: string
}

interface ButtonSettingProps extends SettingItemProps {
  buttonText: string
  onClick: () => void
  variant?: 'default' | 'destructive' | 'outline' | 'secondary' | 'ghost' | 'link'
  disabled?: boolean
}

// 基础设置项容器
export function SettingItem({ title, description, className, children }: SettingItemProps) {
  return (
    <div className={cn("flex items-center justify-between py-4", className)}>
      <div className="flex-1 pr-4">
        <Label className="text-sm font-medium text-foreground">
          {title}
        </Label>
        {description && (
          <p className="text-sm text-muted-foreground mt-1">
            {description}
          </p>
        )}
      </div>
      <div className="flex-shrink-0">
        {children}
      </div>
    </div>
  )
}

// 开关设置项
export function ToggleSetting({ title, description, checked, onCheckedChange, className }: ToggleSettingProps) {
  return (
    <SettingItem title={title} description={description} className={className}>
      <Switch
        checked={checked}
        onCheckedChange={onCheckedChange}
      />
    </SettingItem>
  )
}

// 选择器设置项
export function SelectSetting({ 
  title, 
  description, 
  value, 
  onValueChange, 
  options, 
  placeholder = "选择选项...",
  className 
}: SelectSettingProps) {
  return (
    <SettingItem title={title} description={description} className={className}>
      <Select value={value} onValueChange={onValueChange}>
        <SelectTrigger className="w-48">
          <SelectValue placeholder={placeholder} />
        </SelectTrigger>
        <SelectContent>
          {options.map((option) => (
            <SelectItem key={option.value} value={option.value}>
              {option.label}
            </SelectItem>
          ))}
        </SelectContent>
      </Select>
    </SettingItem>
  )
}

// 输入框设置项
export function InputSetting({ 
  title, 
  description, 
  value, 
  onChange, 
  type = 'text',
  placeholder,
  className 
}: InputSettingProps) {
  return (
    <SettingItem title={title} description={description} className={className}>
      <Input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        className="w-48"
      />
    </SettingItem>
  )
}

// 数字输入设置项
export function NumberSetting({ 
  title, 
  description, 
  value, 
  onValueChange, 
  min,
  max,
  placeholder,
  className 
}: NumberSettingProps) {
  const handleChange = (inputValue: string) => {
    const numValue = parseInt(inputValue, 10)
    if (!isNaN(numValue)) {
      // 应用范围限制
      let finalValue = numValue
      if (min !== undefined && finalValue < min) finalValue = min
      if (max !== undefined && finalValue > max) finalValue = max
      onValueChange(finalValue)
    }
  }

  return (
    <SettingItem title={title} description={description} className={className}>
      <Input
        type="number"
        value={value}
        onChange={(e) => handleChange(e.target.value)}
        min={min}
        max={max}
        placeholder={placeholder}
        className="w-48"
      />
    </SettingItem>
  )
}

// 按钮设置项
export function ButtonSetting({ 
  title, 
  description, 
  buttonText, 
  onClick, 
  variant = 'default',
  disabled = false,
  className 
}: ButtonSettingProps) {
  return (
    <SettingItem title={title} description={description} className={className}>
      <Button
        variant={variant}
        onClick={onClick}
        disabled={disabled}
      >
        {buttonText}
      </Button>
    </SettingItem>
  )
}

// 设置分组
interface SettingGroupProps {
  title: string
  description?: string
  children: React.ReactNode
}

export function SettingGroup({ title, description, children }: SettingGroupProps) {
  return (
    <div className="space-y-2">
      <div className="pb-2">
        <h3 className="text-lg font-semibold text-foreground">{title}</h3>
        {description && (
          <p className="text-sm text-muted-foreground mt-1">{description}</p>
        )}
      </div>
      <div className="space-y-1 border-l-2 border-border pl-4">
        {children}
      </div>
    </div>
  )
} 