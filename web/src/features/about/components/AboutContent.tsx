import { useState } from 'react'
import { toast } from 'sonner'
import { Button } from '@/components/ui/button'
import { ScrollArea } from '@/components/ui/scroll-area'
import { checkForUpdates, downloadUpdate, installUpdate } from '@/lib/updaterApi'
import { useTranslation } from '@/lib/i18n'

type UpdateStatus = {
  available: boolean
  version?: string
  downloadUrl?: string
  releaseNotes?: string
} | null

export function AboutContent() {
  const { t } = useTranslation()
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
        toast.success(t('about.update.newVersionFound', { version: result.version || '' }))
      } else {
        toast.success(t('about.update.latestVersion'))
      }
    } catch (error) {
      toast.error(error instanceof Error ? error.message : t('about.update.checkFailed'))
    } finally {
      setIsChecking(false)
    }
  }

  const handleDownloadUpdate = async () => {
    if (!updateStatus?.downloadUrl) {
      toast.error(t('about.update.noDownloadUrl'))
      return
    }
    setIsDownloading(true)
    try {
      const result = await downloadUpdate(updateStatus.downloadUrl)
      if (result.success) {
        try {
          await installUpdate(false)
          toast.success(t('about.update.updateDownloaded'))
        } catch (installError) {
          toast.error(
            installError instanceof Error ? installError.message : t('about.update.installFailed')
          )
        }
      } else {
        toast.error(result.message || t('about.update.downloadFailed'))
      }
    } catch (error) {
      toast.error(error instanceof Error ? error.message : t('about.update.downloadFailed'))
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
            <h1 className='text-2xl font-bold text-foreground'>{t('about.title')}</h1>
            <p className='mt-1 text-muted-foreground'>{t('about.description')}</p>
          </div>

          <div className='space-y-8'>
            {/* 应用程序信息 */}
            <div className='space-y-4'>
              <div>
                <h3 className='text-lg font-semibold text-foreground'>
                  {t('about.appInfo.title')}
                </h3>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('about.appInfo.description')}
                </p>
              </div>

              <div className='content-panel'>
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
                      {t('about.appInfo.appName')}
                    </div>
                    <div className='text-sm text-muted-foreground'>
                      {t('about.appInfo.appDescription')}
                    </div>
                  </div>
                </div>

                <div className='py-2'>
                  <p className='text-sm text-muted-foreground'>
                    {t('about.appInfo.communityText')}
                  </p>
                </div>
              </div>
            </div>

            {/* 版本更新 */}
            <div className='space-y-4'>
              <div>
                <h3 className='text-lg font-semibold text-foreground'>{t('about.update.title')}</h3>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('about.update.description')}
                </p>
              </div>

              <div className='content-panel'>
                <div className='flex items-center justify-between py-2'>
                  <div className='flex-1 pr-4'>
                    <div className='text-sm font-medium text-foreground'>
                      {t('about.update.checkUpdate')}
                    </div>
                    <p className='mt-1 text-sm text-muted-foreground'>
                      {t('about.update.checkUpdateDescription')}
                    </p>
                  </div>
                  <div className='flex flex-shrink-0 items-center gap-2'>
                    <Button onClick={handleCheckForUpdates} disabled={isChecking} size='sm'>
                      {isChecking ? t('about.update.checking') : t('about.update.checkUpdate')}
                    </Button>
                    {updateStatus?.available && (
                      <Button onClick={handleDownloadUpdate} disabled={isDownloading} size='sm'>
                        {isDownloading
                          ? t('about.update.updating')
                          : t('about.update.downloadAndInstall')}
                      </Button>
                    )}
                  </div>
                </div>

                {/* 更新信息（仅有更新时显示） */}
                {updateStatus?.available && (
                  <div className='rounded-lg border border-accent/40 bg-accent/15 p-3'>
                    <div className='text-sm'>
                      {t('about.update.newVersionFound', { version: updateStatus.version || '' })}
                      {updateStatus.releaseNotes && showNotes && (
                        <pre className='mt-2 text-xs whitespace-pre-wrap text-muted-foreground'>
                          {updateStatus.releaseNotes}
                        </pre>
                      )}
                    </div>
                    {updateStatus.releaseNotes && (
                      <div className='mt-2'>
                        <Button variant='outline' size='sm' onClick={() => setShowNotes((v) => !v)}>
                          {showNotes ? t('about.update.hideNotes') : t('about.update.showNotes')}
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
                <h3 className='text-lg font-semibold text-foreground'>
                  {t('about.resources.title')}
                </h3>
                <p className='mt-1 text-sm text-muted-foreground'>
                  {t('about.resources.description')}
                </p>
              </div>

              <div className='content-panel'>
                <div className='grid gap-3 sm:grid-cols-3'>
                  <a
                    className='flex flex-col items-center gap-2 rounded-md border px-4 py-3 text-center transition hover:bg-accent hover:text-accent-foreground'
                    href='https://chaniok.github.io/SpinningMomo'
                    target='_blank'
                    rel='noreferrer'
                  >
                    <div className='text-sm font-medium'>
                      {t('about.resources.documentation.title')}
                    </div>
                    <div className='text-xs text-muted-foreground'>
                      {t('about.resources.documentation.description')}
                    </div>
                  </a>
                  <a
                    className='flex flex-col items-center gap-2 rounded-md border px-4 py-3 text-center transition hover:bg-accent hover:text-accent-foreground'
                    href='https://github.com/ChanIok/SpinningMomo/releases/latest'
                    target='_blank'
                    rel='noreferrer'
                  >
                    <div className='text-sm font-medium'>
                      {t('about.resources.githubRelease.title')}
                    </div>
                    <div className='text-xs text-muted-foreground'>
                      {t('about.resources.githubRelease.description')}
                    </div>
                  </a>
                  <a
                    className='flex flex-col items-center gap-2 rounded-md border px-4 py-3 text-center transition hover:bg-accent hover:text-accent-foreground'
                    href='https://github.com/ChanIok/SpinningMomo/blob/main/LICENSE'
                    target='_blank'
                    rel='noreferrer'
                  >
                    <div className='text-sm font-medium'>{t('about.resources.license.title')}</div>
                    <div className='text-xs text-muted-foreground'>
                      {t('about.resources.license.description')}
                    </div>
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
