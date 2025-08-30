import { useState } from 'react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { checkForUpdates, downloadUpdate, installUpdate } from '@/lib/updaterApi'

type UpdateStatus = {
  available: boolean
  version?: string
  downloadUrl?: string
  releaseNotes?: string
} | null

export function AboutContent() {
  const [updateStatus, setUpdateStatus] = useState<UpdateStatus>(null)
  const [isChecking, setIsChecking] = useState(false)
  const [isDownloading, setIsDownloading] = useState(false)
  const [showNotes, setShowNotes] = useState(false)

  const handleCheckForUpdates = async () => {
    setIsChecking(true)
    try {
      const result = await checkForUpdates()
      setUpdateStatus(result)
      if (result.available) {
        toast.success(`发现新版本: ${result.version}`)
      } else {
        toast.success('当前已是最新版本')
      }
    } catch (error) {
      toast.error(error instanceof Error ? error.message : '检查更新失败')
    } finally {
      setIsChecking(false)
    }
  }

  const handleDownloadUpdate = async () => {
    if (!updateStatus?.downloadUrl) {
      toast.error('没有可用的下载链接')
      return
    }
    setIsDownloading(true)
    try {
      const result = await downloadUpdate(updateStatus.downloadUrl)
      if (result.success) {
        try {
          await installUpdate(false)
          toast.success('更新已下载并将在下次程序重启时自动应用')
        } catch (installError) {
          toast.error(installError instanceof Error ? installError.message : '安装更新失败')
        }
      } else {
        toast.error(result.message || '下载更新失败')
      }
    } catch (error) {
      toast.error(error instanceof Error ? error.message : '下载更新失败')
    } finally {
      setIsDownloading(false)
    }
  }

  return (
    <div className='flex h-full flex-col'>
      <ScrollArea className='overflow-auto'>
        <div className='mx-auto max-w-4xl p-4'>
          {/* 页面标题 */}
          <div className='mb-6'>
            <h1 className='text-2xl font-bold text-foreground'>关于</h1>
            <p className='mt-1 text-muted-foreground'>了解应用程序信息、检查更新和相关资源</p>
          </div>

          <div className='space-y-8'>
            {/* 应用程序信息 */}
            <div className='space-y-4'>
              <div>
                <h3 className='text-lg font-semibold text-foreground'>应用程序信息</h3>
                <p className='mt-1 text-sm text-muted-foreground'>关于旋转吧大喵的基本信息</p>
              </div>

              <div className='space-y-4 rounded-md border border-border bg-card p-4'>
                <div className='flex items-center gap-4 py-2'>
                  {/* <img
                    src='/logo.png'
                    alt='SpinningMomo Logo'
                    className='h-12 w-12 rounded-lg border bg-muted/30'
                    onError={(e) => {
                      const target = e.currentTarget as HTMLImageElement
                      target.style.display = 'none'
                    }}
                  /> */}
                  
                  <div className='flex-1'>
                    <div className='text-lg font-semibold text-foreground'>
                      旋转吧大喵
                    </div>
                    <div className='text-sm text-muted-foreground'>
                      一个为《无限暖暖》提升摄影体验的窗口调整工具
                    </div>
                  </div>
                </div>

                <div className='py-2'>
                  <p className='text-sm text-muted-foreground'>
                    由开源社区与 AI 协作完成，感谢你的支持。
                  </p>
                </div>
              </div>
            </div>

            {/* 版本更新 */}
            <div className='space-y-4'>
              <div>
                <h3 className='text-lg font-semibold text-foreground'>版本更新</h3>
                <p className='mt-1 text-sm text-muted-foreground'>检查和安装应用程序更新</p>
              </div>

              <div className='space-y-4 rounded-md border border-border bg-card p-4'>
                <div className='flex items-center justify-between py-2'>
                  <div className='flex-1 pr-4'>
                    <div className='text-sm font-medium text-foreground'>检查更新</div>
                    <p className='mt-1 text-sm text-muted-foreground'>
                      检查是否有可用的应用程序更新
                    </p>
                  </div>
                  <div className='flex flex-shrink-0 items-center gap-2'>
                    <Button onClick={handleCheckForUpdates} disabled={isChecking} size='sm'>
                      {isChecking ? '检查中...' : '检查更新'}
                    </Button>
                    {updateStatus?.available && (
                      <Button onClick={handleDownloadUpdate} disabled={isDownloading} size='sm'>
                        {isDownloading ? '更新中...' : '下载并安装'}
                      </Button>
                    )}
                  </div>
                </div>

                {/* 更新信息（仅有更新时显示） */}
                {updateStatus?.available && (
                  <div className='rounded-lg border border-accent/40 bg-accent/15 p-3'>
                    <div className='text-sm'>
                      发现新版本：<b>v{updateStatus.version}</b>
                      {updateStatus.releaseNotes && showNotes && (
                        <pre className='mt-2 text-xs whitespace-pre-wrap text-muted-foreground'>
                          {updateStatus.releaseNotes}
                        </pre>
                      )}
                    </div>
                    {updateStatus.releaseNotes && (
                      <div className='mt-2'>
                        <Button variant='outline' size='sm' onClick={() => setShowNotes((v) => !v)}>
                          {showNotes ? '收起说明' : '查看说明'}
                        </Button>
                      </div>
                    )}
                  </div>
                )}
              </div>
            </div>

            {/* 相关资源 */}
            <div className='space-y-4'>
              <div>
                <h3 className='text-lg font-semibold text-foreground'>相关资源</h3>
                <p className='mt-1 text-sm text-muted-foreground'>访问文档、源代码和许可证信息</p>
              </div>

              <div className='space-y-4 rounded-md border border-border bg-card p-4'>
                <div className='grid gap-3 sm:grid-cols-3'>
                  <a
                    className='flex flex-col items-center gap-2 rounded-md border px-4 py-3 text-center transition hover:bg-accent hover:text-accent-foreground'
                    href='https://chaniok.github.io/SpinningMomo'
                    target='_blank'
                    rel='noreferrer'
                  >
                    <div className='text-sm font-medium'>使用文档</div>
                    <div className='text-xs text-muted-foreground'>查看完整的使用指南</div>
                  </a>
                  <a
                    className='flex flex-col items-center gap-2 rounded-md border px-4 py-3 text-center transition hover:bg-accent hover:text-accent-foreground'
                    href='https://github.com/ChanIok/SpinningMomo/releases/latest'
                    target='_blank'
                    rel='noreferrer'
                  >
                    <div className='text-sm font-medium'>GitHub Release</div>
                    <div className='text-xs text-muted-foreground'>下载最新版本</div>
                  </a>
                  <a
                    className='flex flex-col items-center gap-2 rounded-md border px-4 py-3 text-center transition hover:bg-accent hover:text-accent-foreground'
                    href='https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE'
                    target='_blank'
                    rel='noreferrer'
                  >
                    <div className='text-sm font-medium'>MIT License</div>
                    <div className='text-xs text-muted-foreground'>查看开源许可证</div>
                  </a>
                </div>
              </div>
            </div>
          </div>
        </div>
      </ScrollArea>
    </div>
  )
}
