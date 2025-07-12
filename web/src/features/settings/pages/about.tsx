import { ExternalLink, Github, Heart } from 'lucide-react'
import { useSettingsStore } from '../store/settings-store'
import { Button } from '@/components/ui/button'

export function AboutPage() {
  const appInfo = useSettingsStore(state => state.appInfo)
  const isLoading = useSettingsStore(state => state.isLoading)
  const isInitialized = useSettingsStore(state => state.isInitialized)

  // 显示加载状态（仅在未初始化时）
  if (!isInitialized && isLoading) {
    return (
      <div className="p-6 flex items-center justify-center">
        <div className="text-center">
          <div className="h-8 w-8 animate-spin rounded-full border-4 border-muted border-t-primary mx-auto"></div>
          <p className="text-sm text-muted-foreground mt-2">加载应用信息中...</p>
        </div>
      </div>
    )
  }

  return (
    <div className="p-6 max-w-4xl">
      {/* 页面标题 */}
      <div className="mb-8">
        <h1 className="text-2xl font-bold text-foreground">关于 SpinningMomo</h1>
        <p className="text-muted-foreground mt-1">
          应用程序信息和版本详情
        </p>
      </div>

      <div className="space-y-8">
        {/* 应用信息卡片 */}
        <div className="rounded-lg border border-border bg-card p-6">
          <div className="flex items-start space-x-6">
            {/* 应用图标 */}
            <div className="flex-shrink-0">
              <div className="h-16 w-16 rounded-lg bg-primary/10 flex items-center justify-center">
                <svg
                  className="h-8 w-8 text-primary"
                  fill="none"
                  stroke="currentColor"
                  viewBox="0 0 24 24"
                >
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth={2}
                    d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15"
                  />
                </svg>
              </div>
            </div>

            {/* 应用信息 */}
            <div className="flex-1">
              <h2 className="text-xl font-semibold text-foreground">SpinningMomo</h2>
              <p className="text-muted-foreground mt-1">
                一个现代化的桌面应用程序，为用户提供优雅的体验。
              </p>
              
              {appInfo ? (
                <div className="mt-4 space-y-2">
                  <div className="flex items-center space-x-4 text-sm">
                    <span className="text-muted-foreground">版本:</span>
                    <span className="font-mono text-foreground">{appInfo.version}</span>
                  </div>
                  <div className="flex items-center space-x-4 text-sm">
                    <span className="text-muted-foreground">构建日期:</span>
                    <span className="font-mono text-foreground">
                      {new Date(appInfo.buildDate).toLocaleDateString('zh-CN')}
                    </span>
                  </div>
                  <div className="flex items-center space-x-4 text-sm">
                    <span className="text-muted-foreground">平台:</span>
                    <span className="font-mono text-foreground">{appInfo.platform}</span>
                  </div>
                  <div className="flex items-center space-x-4 text-sm">
                    <span className="text-muted-foreground">架构:</span>
                    <span className="font-mono text-foreground">{appInfo.architecture}</span>
                  </div>
                </div>
              ) : (
                <div className="mt-4 text-sm text-muted-foreground">
                  应用信息加载中...
                </div>
              )}
            </div>
          </div>
        </div>

        {/* 许可证信息 */}
        <div className="rounded-lg border border-border bg-card p-6">
          <h3 className="text-lg font-semibold text-foreground mb-4">许可证</h3>
          <div className="space-y-3">
            <p className="text-sm text-muted-foreground">
              本软件基于 MIT 许可证发布，允许自由使用、修改和分发。
            </p>
            <div className="flex space-x-2">
              <Button variant="outline" size="sm">
                <ExternalLink className="h-4 w-4 mr-2" />
                查看许可证
              </Button>
            </div>
          </div>
        </div>

        {/* 开源信息 */}
        <div className="rounded-lg border border-border bg-card p-6">
          <h3 className="text-lg font-semibold text-foreground mb-4">开源</h3>
          <div className="space-y-3">
            <p className="text-sm text-muted-foreground">
              SpinningMomo 是一个开源项目，我们欢迎社区贡献。
            </p>
            <div className="flex space-x-2">
              <Button variant="outline" size="sm">
                <Github className="h-4 w-4 mr-2" />
                访问源代码
              </Button>
              <Button variant="outline" size="sm">
                <ExternalLink className="h-4 w-4 mr-2" />
                报告问题
              </Button>
            </div>
          </div>
        </div>

        {/* 致谢 */}
        <div className="rounded-lg border border-border bg-card p-6">
          <h3 className="text-lg font-semibold text-foreground mb-4">致谢</h3>
          <div className="space-y-3">
            <p className="text-sm text-muted-foreground">
              感谢所有开源项目的贡献者，特别是以下技术栈：
            </p>
            <div className="grid grid-cols-2 md:grid-cols-3 gap-3 text-sm">
              <div className="flex items-center space-x-2">
                <div className="h-2 w-2 rounded-full bg-blue-500"></div>
                <span>React</span>
              </div>
              <div className="flex items-center space-x-2">
                <div className="h-2 w-2 rounded-full bg-blue-600"></div>
                <span>TypeScript</span>
              </div>
              <div className="flex items-center space-x-2">
                <div className="h-2 w-2 rounded-full bg-cyan-500"></div>
                <span>Tailwind CSS</span>
              </div>
              <div className="flex items-center space-x-2">
                <div className="h-2 w-2 rounded-full bg-purple-500"></div>
                <span>Vite</span>
              </div>
              <div className="flex items-center space-x-2">
                <div className="h-2 w-2 rounded-full bg-orange-500"></div>
                <span>Zustand</span>
              </div>
              <div className="flex items-center space-x-2">
                <div className="h-2 w-2 rounded-full bg-gray-500"></div>
                <span>WebView2</span>
              </div>
            </div>
            <div className="flex items-center space-x-2 text-sm text-muted-foreground mt-4">
              <Heart className="h-4 w-4 text-red-500" />
              <span>用 ❤️ 制作</span>
            </div>
          </div>
        </div>

        {/* 联系信息 */}
        <div className="rounded-lg border border-border bg-card p-6">
          <h3 className="text-lg font-semibold text-foreground mb-4">联系我们</h3>
          <div className="space-y-3">
            <p className="text-sm text-muted-foreground">
              如果您有任何问题或建议，请通过以下方式联系我们：
            </p>
            <div className="flex flex-col space-y-2">
              <Button variant="outline" size="sm" className="justify-start w-fit">
                <ExternalLink className="h-4 w-4 mr-2" />
                官方网站
              </Button>
              <Button variant="outline" size="sm" className="justify-start w-fit">
                <ExternalLink className="h-4 w-4 mr-2" />
                用户文档
              </Button>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
} 