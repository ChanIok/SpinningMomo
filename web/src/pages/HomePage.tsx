import { useState } from 'react'
import { useTranslation } from '@/lib/i18n'
import { scanInfinityNikkiPhotos } from '@/plugins/infinity_nikki'
import type { InfinityNikkiScanResult } from '@/plugins/infinity_nikki'

export function HomePage() {
  const { t } = useTranslation()
  const [isScanning, setIsScanning] = useState(false)
  const [scanResult, setScanResult] = useState<InfinityNikkiScanResult | null>(null)

  const handleScanInfinityNikki = async () => {
    setIsScanning(true)
    setScanResult(null)

    try {
      console.log('🎮 触发Infinity Nikki照片扫描')
      const result = await scanInfinityNikkiPhotos()
      setScanResult(result)

      if (result.success) {
        console.log('✅ 扫描成功完成')
      } else {
        console.error('❌ 扫描失败:', result.error)
      }
    } catch (error) {
      console.error('❌ 扫描过程中出现异常:', error)
      setScanResult({
        success: false,
        error: error instanceof Error ? error.message : '未知错误',
      })
    } finally {
      setIsScanning(false)
    }
  }

  return (
    <div className='flex h-full flex-col'>
      <div className='mt-auto mb-32 ml-16'>
        <div className='mb-8 text-2xl text-muted-foreground'>{t('home.welcome')}</div>

        {/* Infinity Nikki 测试区域 */}
        <div className='w-96 rounded-lg border border-gray-200 bg-white p-6 shadow-sm'>
          <h3 className='mb-2 text-lg font-semibold'>🎮 Infinity Nikki 测试</h3>
          <p className='mb-4 text-sm text-gray-600'>测试Infinity Nikki照片扫描功能，验证后端响应</p>

          <button
            onClick={handleScanInfinityNikki}
            disabled={isScanning}
            className='w-full rounded bg-blue-500 px-4 py-2 text-white hover:bg-blue-600 disabled:cursor-not-allowed disabled:bg-gray-400'
          >
            {isScanning ? '📸 扫描中...' : '📸 扫描Infinity Nikki照片'}
          </button>

          {/* 扫描结果显示 */}
          {scanResult && (
            <div
              className={`mt-4 rounded p-3 ${scanResult.success ? 'border border-green-200 bg-green-50' : 'border border-red-200 bg-red-50'}`}
            >
              {scanResult.success ? (
                <div>
                  <div className='mb-2 font-semibold text-green-800'>✅ 扫描成功</div>
                  {scanResult.scanResult && (
                    <div className='space-y-1 text-sm text-green-700'>
                      <div>游戏目录: {scanResult.gameDirectory}</div>
                      <div>总文件: {scanResult.scanResult.totalFiles}</div>
                      <div>新增: {scanResult.scanResult.newItems}</div>
                      <div>更新: {scanResult.scanResult.updatedItems}</div>
                      <div>耗时: {scanResult.scanResult.scanDuration}</div>
                      {scanResult.scanResult.errors.length > 0 && (
                        <div>错误: {scanResult.scanResult.errors.length}</div>
                      )}
                    </div>
                  )}
                </div>
              ) : (
                <div>
                  <div className='mb-1 font-semibold text-red-800'>❌ 扫描失败</div>
                  <div className='text-sm text-red-700'>{scanResult.error}</div>
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  )
}

export default HomePage
