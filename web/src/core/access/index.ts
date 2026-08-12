import { call } from '@/core/rpc'
import { isWebView } from '@/core/env'

type AccessLevel = 'unknown' | 'local' | 'lan'

// 在权限探测完成前不假定调用者拥有宿主机权限。
let accessLevel: AccessLevel = 'unknown'

// 启动阶段通过 RPC 确认调用者来自 WebView、本机回环还是已认证 LAN。
export async function initializeAccessLevel(): Promise<void> {
  try {
    // 后端根据真实传输来源返回访问等级，前端不自行解析 IP 或 Cookie。
    const result = await call<unknown>('system.getAccessLevel')
    if (result !== 'local' && result !== 'lan') {
      // 异常返回继续保持最小权限，避免误展示宿主机操作。
      console.warn('Invalid access level response:', result)
      accessLevel = 'unknown'
      return
    }

    accessLevel = result
  } catch (error) {
    // WebView2 本身就是本机调用者，可以在桥接探测失败时保留本机启动路径；
    // 普通浏览器则保持 unknown，避免把宿主机操作 UI 展示给远端用户。
    console.warn('Failed to resolve HTTP access level:', error)
    accessLevel = isWebView() ? 'local' : 'unknown'
  }
}

// 判断当前页面是否可以触发宿主机文件、剪贴板和系统操作。
export function isLocalAccess(): boolean {
  return accessLevel === 'local'
}
