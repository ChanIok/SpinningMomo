import { useState } from 'react'
import { toast } from 'sonner'
import { useSettingsStore } from '@/lib/settings'
import { Button } from '@/components/ui/button'
import { checkForUpdates, downloadUpdate, installUpdate } from '@/lib/updater-api'

type UpdateStatus = {
  available: boolean
  version?: string
  downloadUrl?: string
  releaseNotes?: string
} | null

export function AboutContent() {
  const { appSettings } = useSettingsStore()
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
    <div className='w-full max-w-[768px] p-6'>
      {/* 顶部信息卡 */}
      <div className='flex items-center gap-4 rounded-xl border bg-background/60 p-5 shadow-sm backdrop-blur supports-[backdrop-filter]:bg-background/40'>
        <img
          src='/logo.png'
          alt='SpinningMomo Logo'
          className='h-12 w-12 rounded-lg border bg-muted/30'
          onError={(e) => {
            const target = e.currentTarget as HTMLImageElement
            target.style.display = 'none'
          }}
        />
        <div className='flex-1'>
          <div className='text-2xl font-bold'>旋转吧大喵（SpinningMomo）</div>
          <div className='text-sm text-muted-foreground'>
            一个为《无限暖暖》提升摄影体验的窗口调整工具
          </div>
        </div>
        <div className='ml-auto flex items-center gap-2'>
          <span className='rounded-md border px-2 py-1 text-xs text-muted-foreground'>
            v{appSettings.version}
          </span>
          <Button onClick={handleCheckForUpdates} disabled={isChecking}>
            {isChecking ? '检查中...' : '检查更新'}
          </Button>
          {updateStatus?.available && (
            <Button onClick={handleDownloadUpdate} disabled={isDownloading}>
              {isDownloading ? '更新中...' : '下载并安装'}
            </Button>
          )}
        </div>
      </div>

      {/* 更新条幅（仅有更新时显示） */}
      {updateStatus?.available && (
        <div className='flex items-start justify-between gap-4 rounded-lg border border-accent/40 bg-accent/15 p-4'>
          <div className='text-sm'>
            发现新版本：<b>v{updateStatus.version}</b>
            {updateStatus.releaseNotes && showNotes && (
              <pre className='mt-2 text-xs whitespace-pre-wrap text-muted-foreground'>
                {updateStatus.releaseNotes}
              </pre>
            )}
          </div>
          <div className='flex gap-2'>
            {updateStatus.releaseNotes && (
              <Button variant='outline' onClick={() => setShowNotes((v) => !v)}>
                {showNotes ? '收起说明' : '查看说明'}
              </Button>
            )}
            <Button onClick={handleDownloadUpdate} disabled={isDownloading}>
              {isDownloading ? '更新中...' : '下载并安装'}
            </Button>
          </div>
        </div>
      )}

      {/* 链接区 */}
      <div className='flex flex-wrap gap-3'>
        <a
          className='rounded-md border px-3 py-2 transition hover:bg-accent hover:text-accent-foreground'
          href='https://chaniok.github.io/SpinningMomo'
          target='_blank'
          rel='noreferrer'
        >
          使用文档
        </a>
        <a
          className='rounded-md border px-3 py-2 transition hover:bg-accent hover:text-accent-foreground'
          href='https://github.com/ChanIok/SpinningMomo/releases/latest'
          target='_blank'
          rel='noreferrer'
        >
          GitHub Release
        </a>
        <a
          className='rounded-md border px-3 py-2 transition hover:bg-accent hover:text-accent-foreground'
          href='https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE'
          target='_blank'
          rel='noreferrer'
        >
          MIT License
        </a>
      </div>

      {/* 页脚致谢 */}
      <div className='text-xs text-muted-foreground'>由开源社区与 AI 协作完成，感谢你的支持。</div>
    </div>
  )
}
