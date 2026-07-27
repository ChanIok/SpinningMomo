/**
 * WebView2 类型声明
 */
interface Window {
  chrome?: {
    webview?: {
      postMessage(message: any): void
      postMessageWithAdditionalObjects(message: any, additionalObjects: object[]): void
      addEventListener(event: 'message', handler: (event: MessageEvent) => void): void
      removeEventListener(event: 'message', handler: (event: MessageEvent) => void): void
    }
  }
}
