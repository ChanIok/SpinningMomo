// 扩展 Window 接口以支持 WebView2 的 chrome.webview API
declare global {
  interface Window {
    chrome?: {
      webview?: {
        addEventListener: (event: string, handler: (event: MessageEvent) => void) => void
        postMessage: (message: any) => void
      }
    }
  }
}

export {} 