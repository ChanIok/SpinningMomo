// 修饰键映射
export const MODIFIER_MAP = {
  shift: 1,
  ctrl: 2,
  alt: 4,
} as const

// 修饰键显示映射
export const MODIFIER_DISPLAY: Record<number, string> = {
  1: 'Shift',
  2: 'Ctrl',
  4: 'Alt',
  3: 'Ctrl + Shift',
  5: 'Shift + Alt',
  6: 'Ctrl + Alt',
  7: 'Ctrl + Shift + Alt',
}

// 主键映射
export const KEY_CODE_MAP: Record<number, string> = {
  // 字母
  65: 'A',
  66: 'B',
  67: 'C',
  68: 'D',
  69: 'E',
  70: 'F',
  71: 'G',
  72: 'H',
  73: 'I',
  74: 'J',
  75: 'K',
  76: 'L',
  77: 'M',
  78: 'N',
  79: 'O',
  80: 'P',
  81: 'Q',
  82: 'R',
  83: 'S',
  84: 'T',
  85: 'U',
  86: 'V',
  87: 'W',
  88: 'X',
  89: 'Y',
  90: 'Z',

  // 数字
  48: '0',
  49: '1',
  50: '2',
  51: '3',
  52: '4',
  53: '5',
  54: '6',
  55: '7',
  56: '8',
  57: '9',

  // 功能键
  112: 'F1',
  113: 'F2',
  114: 'F3',
  115: 'F4',
  116: 'F5',
  117: 'F6',
  118: 'F7',
  119: 'F8',
  120: 'F9',
  121: 'F10',
  122: 'F11',
  123: 'F12',

  // 特殊键
  8: 'Backspace',
  9: 'Tab',
  13: 'Enter',
  27: 'Escape',
  32: 'Space',
  37: 'Left',
  38: 'Up',
  39: 'Right',
  40: 'Down',
  188: ',',
  190: '.',
  191: '/',
  186: ';',
  222: "'",
  219: '[',
  221: ']',
  189: '-',
  187: '=',
  220: '\\',

  // 系统特殊键
  44: 'PrintScreen',
  145: 'ScrollLock',
  19: 'Pause',
  45: 'Insert',
  46: 'Delete',
  36: 'Home',
  35: 'End',
  33: 'PageUp',
  34: 'PageDown',

  // 数字键盘
  96: 'Numpad0',
  97: 'Numpad1',
  98: 'Numpad2',
  99: 'Numpad3',
  100: 'Numpad4',
  101: 'Numpad5',
  102: 'Numpad6',
  103: 'Numpad7',
  104: 'Numpad8',
  105: 'Numpad9',
  106: 'Numpad*',
  107: 'Numpad+',
  109: 'Numpad-',
  110: 'Numpad.',
  111: 'Numpad/',
}

// 反向映射：字符到键码
export const KEY_NAME_MAP: Record<string, number> = Object.entries(KEY_CODE_MAP).reduce(
  (acc, [code, name]) => ({ ...acc, [name]: parseInt(code) }),
  {}
)

// 计算修饰键值
export const calculateModifiers = (shift: boolean, ctrl: boolean, alt: boolean): number => {
  return (
    (shift ? MODIFIER_MAP.shift : 0) | (ctrl ? MODIFIER_MAP.ctrl : 0) | (alt ? MODIFIER_MAP.alt : 0)
  )
}

// 格式化快捷键显示文本
export const formatHotkeyDisplay = (modifiers: number, key: number): string => {
  // 如果既没有修饰键也没有主键，返回"未设置"
  if (!modifiers && !key) return '未设置'

  const modifierText = MODIFIER_DISPLAY[modifiers] || ''
  const keyText = key ? KEY_CODE_MAP[key] || String.fromCharCode(key) : ''

  // 如果有修饰键但没有主键，只显示修饰键
  if (modifiers && !key) return modifierText

  // 如果有修饰键和主键，显示组合
  if (modifierText && keyText) return `${modifierText} + ${keyText}`

  // 如果只有主键，只显示主键
  return keyText
}

// 解析显示文本为修饰键和键码
export const parseHotkeyDisplay = (display: string): { modifiers: number; key: number } => {
  if (display === '未设置' || !display) {
    return { modifiers: 0, key: 0 }
  }

  // 分割修饰键和主键
  const parts = display.split(' + ')

  if (parts.length === 1) {
    // 只有主键
    const keyName = parts[0]
    return { modifiers: 0, key: KEY_NAME_MAP[keyName] || 0 }
  }

  // 有修饰键和主键
  const keyName = parts[parts.length - 1]
  const modifierNames = parts.slice(0, -1)

  let modifiers = 0
  for (const name of modifierNames) {
    switch (name) {
      case 'Shift':
        modifiers |= MODIFIER_MAP.shift
        break
      case 'Ctrl':
        modifiers |= MODIFIER_MAP.ctrl
        break
      case 'Alt':
        modifiers |= MODIFIER_MAP.alt
        break
    }
  }

  return {
    modifiers,
    key: KEY_NAME_MAP[keyName] || 0,
  }
}
