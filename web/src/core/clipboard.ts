import { call } from '@/core/rpc'

export async function readClipboardText(): Promise<string | null> {
  try {
    const result = await call<string | null>('clipboard.readText', {})
    return result
  } catch (error) {
    console.error('Failed to read clipboard text:', error)
    throw new Error('读取剪贴板失败')
  }
}
