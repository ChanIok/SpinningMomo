import { useState } from 'react'
import { useTranslation } from '@/lib/i18n'
import { scanInfinityNikkiPhotos } from '@/plugins/infinity_nikki'
import type { InfinityNikkiScanResult } from '@/plugins/infinity_nikki'
import { getStaticUrl } from '@/lib/environment'

interface PerformanceTestResult {
  loadTime: number
  timestamp: number
  imageUrl: string
  success: boolean
  error?: string
}

interface StressTestSummary {
  totalRequests: number
  successfulRequests: number
  failedRequests: number
  totalDuration: number
  averageLoadTime: number
  minLoadTime: number
  maxLoadTime: number
  requestsPerSecond: number
}

export function HomePage() {
  const { t } = useTranslation()
  const [isScanning, setIsScanning] = useState(false)
  const [scanResult, setScanResult] = useState<InfinityNikkiScanResult | null>(null)
  const [isTesting, setIsTesting] = useState(false)
  const [performanceResults, setPerformanceResults] = useState<PerformanceTestResult[]>([])
  const [testProgress, setTestProgress] = useState(0)
  const [testSummary, setTestSummary] = useState<StressTestSummary | null>(null)

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

  const handlePerformanceTest = async () => {
    setIsTesting(true)
    setTestProgress(0)
    setPerformanceResults([])
    setTestSummary(null)

    const TEST_DURATION = 10000 // 10秒
    const CONCURRENT_REQUESTS = 10 // 并发请求数量
    const results: PerformanceTestResult[] = []
    const testStartTime = Date.now()
    let requestCount = 0

    console.log('🚀 开始10秒压力测试（10个并发）...')

    // 创建单个图片加载请求的函数
    const createImageLoadPromise = async (): Promise<PerformanceTestResult> => {
      const currentRequestCount = ++requestCount
      try {
        // 添加随机参数避免缓存
        const randomParam = `nocache=${Date.now()}_${Math.random().toString(36).substring(7)}`
        const imageUrl = getStaticUrl(`/static/thumbnails/PD10202505.webp?${randomParam}`)
        
        const startTime = performance.now()

        // 创建一个新的 Image 对象来测试加载
        const img = new Image()
        
        await new Promise<void>((resolve, reject) => {
          img.onload = () => resolve()
          img.onerror = () => reject(new Error('图片加载失败'))
          img.src = imageUrl
        })

        const endTime = performance.now()
        const loadTime = endTime - startTime

        return {
          loadTime,
          timestamp: Date.now(),
          imageUrl,
          success: true,
        }
      } catch (error) {
        console.error(`❌ 请求 #${currentRequestCount} 失败:`, error)
        return {
          loadTime: 0,
          timestamp: Date.now(),
          imageUrl: '',
          success: false,
          error: error instanceof Error ? error.message : '未知错误',
        }
      }
    }

    try {
      // 持续10秒的压力测试，每次发起10个并发请求
      while (Date.now() - testStartTime < TEST_DURATION) {
        const elapsed = Date.now() - testStartTime
        const progress = (elapsed / TEST_DURATION) * 100
        setTestProgress(Math.min(progress, 100))

        // 创建10个并发请求
        const batchPromises = Array.from({ length: CONCURRENT_REQUESTS }, () => createImageLoadPromise())
        
        // 等待所有并发请求完成
        const batchResults = await Promise.all(batchPromises)
        results.push(...batchResults)
      }

      // 计算统计信息
      const successfulResults = results.filter(r => r.success)
      const failedResults = results.filter(r => !r.success)
      const loadTimes = successfulResults.map(r => r.loadTime)
      const totalDuration = Date.now() - testStartTime

      const summary: StressTestSummary = {
        totalRequests: results.length,
        successfulRequests: successfulResults.length,
        failedRequests: failedResults.length,
        totalDuration,
        averageLoadTime: loadTimes.length > 0 ? loadTimes.reduce((a, b) => a + b, 0) / loadTimes.length : 0,
        minLoadTime: loadTimes.length > 0 ? Math.min(...loadTimes) : 0,
        maxLoadTime: loadTimes.length > 0 ? Math.max(...loadTimes) : 0,
        requestsPerSecond: (results.length / totalDuration) * 1000,
      }

      console.log('🏁 压力测试完成:', summary)
      setPerformanceResults(results)
      setTestSummary(summary)
      setTestProgress(100)
    } catch (error) {
      console.error('❌ 压力测试异常:', error)
    } finally {
      setIsTesting(false)
    }
  }

  return (
    <div className='flex h-full flex-col'>
      <div className='mt-auto mb-32 ml-16'>
        <div className='mb-8 text-2xl text-muted-foreground'>{t('home.welcome')}</div>

        {/* 图片测试 */}
        <div className='w-96 rounded-lg border border-gray-200 bg-white p-6 shadow-sm'>
          <h3 className='mb-2 text-lg font-semibold'>🎨 图片测试</h3>
          <p className='mb-4 text-sm text-gray-600'>测试图片显示功能，验证后端响应</p>
          <img
            src={getStaticUrl('/static/thumbnails/0b/6e/0b6e6bbb133e6c7f_400x400.webp')}
            alt='图片测试'
            className='h-auto w-full'
          />
        </div>

        {/* WebView2 性能测试 */}
        <div className='w-96 rounded-lg border border-purple-200 bg-white p-6 shadow-sm'>
          <h3 className='mb-2 text-lg font-semibold'>⚡ WebView2 压力测试</h3>
          <p className='mb-4 text-sm text-gray-600'>持续10秒的图片加载压力测试，每次并发10个请求，添加随机参数避免缓存</p>

          <button
            onClick={handlePerformanceTest}
            disabled={isTesting}
            className='w-full rounded bg-purple-500 px-4 py-2 text-white hover:bg-purple-600 disabled:cursor-not-allowed disabled:bg-gray-400'
          >
            {isTesting ? `⏱️ 测试中... ${testProgress.toFixed(0)}%` : '⚡ 开始10秒并发测试'}
          </button>

          {/* 进度条 */}
          {isTesting && (
            <div className='mt-4'>
              <div className='h-2 w-full overflow-hidden rounded-full bg-gray-200'>
                <div
                  className='h-full bg-purple-500 transition-all duration-300'
                  style={{ width: `${testProgress}%` }}
                />
              </div>
            </div>
          )}

          {/* 测试摘要 */}
          {testSummary && (
            <div className='mt-4 rounded border border-purple-200 bg-purple-50 p-4'>
              <div className='mb-3 text-lg font-bold text-purple-900'>📊 压力测试报告</div>
              
              <div className='space-y-2 text-sm'>
                {/* 总览 */}
                <div className='rounded bg-white p-3'>
                  <div className='mb-2 font-semibold text-purple-800'>📈 总览</div>
                  <div className='grid grid-cols-2 gap-2 text-xs'>
                    <div>
                      <span className='text-gray-600'>总请求数:</span>
                      <span className='ml-2 font-bold text-purple-900'>{testSummary.totalRequests}</span>
                    </div>
                    <div>
                      <span className='text-gray-600'>成功:</span>
                      <span className='ml-2 font-bold text-green-600'>{testSummary.successfulRequests}</span>
                    </div>
                    <div>
                      <span className='text-gray-600'>失败:</span>
                      <span className='ml-2 font-bold text-red-600'>{testSummary.failedRequests}</span>
                    </div>
                    <div>
                      <span className='text-gray-600'>测试时长:</span>
                      <span className='ml-2 font-bold text-purple-900'>{(testSummary.totalDuration / 1000).toFixed(2)}s</span>
                    </div>
                  </div>
                </div>

                {/* 性能指标 */}
                <div className='rounded bg-white p-3'>
                  <div className='mb-2 font-semibold text-purple-800'>⚡ 性能指标</div>
                  <div className='space-y-1 text-xs'>
                    <div className='flex justify-between'>
                      <span className='text-gray-600'>平均加载时间:</span>
                      <span className='font-bold text-purple-900'>{testSummary.averageLoadTime.toFixed(2)} ms</span>
                    </div>
                    <div className='flex justify-between'>
                      <span className='text-gray-600'>最快:</span>
                      <span className='font-bold text-green-600'>{testSummary.minLoadTime.toFixed(2)} ms</span>
                    </div>
                    <div className='flex justify-between'>
                      <span className='text-gray-600'>最慢:</span>
                      <span className='font-bold text-red-600'>{testSummary.maxLoadTime.toFixed(2)} ms</span>
                    </div>
                    <div className='flex justify-between'>
                      <span className='text-gray-600'>吞吐量:</span>
                      <span className='font-bold text-purple-900'>{testSummary.requestsPerSecond.toFixed(2)} req/s</span>
                    </div>
                  </div>
                </div>

                {/* 成功率 */}
                <div className='rounded bg-white p-3'>
                  <div className='mb-2 font-semibold text-purple-800'>✅ 成功率</div>
                  <div className='flex items-center'>
                    <div className='h-4 flex-1 overflow-hidden rounded-full bg-gray-200'>
                      <div
                        className='h-full bg-gradient-to-r from-green-500 to-green-600'
                        style={{ width: `${(testSummary.successfulRequests / testSummary.totalRequests) * 100}%` }}
                      />
                    </div>
                    <span className='ml-3 text-xs font-bold text-purple-900'>
                      {((testSummary.successfulRequests / testSummary.totalRequests) * 100).toFixed(1)}%
                    </span>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* 详细结果列表 */}
          {performanceResults.length > 0 && (
            <div className='mt-4 rounded border border-gray-200 bg-gray-50 p-3'>
              <div className='mb-2 flex items-center justify-between'>
                <span className='font-semibold text-gray-800'>📋 详细日志</span>
                <span className='text-xs text-gray-600'>{performanceResults.length} 条记录</span>
              </div>
              <div className='max-h-48 space-y-1 overflow-y-auto'>
                {performanceResults.map((result, index) => (
                  <div
                    key={result.timestamp}
                    className={`rounded px-2 py-1 text-xs ${
                      result.success
                        ? 'bg-white text-gray-700'
                        : 'bg-red-50 text-red-700'
                    }`}
                  >
                    <span className='font-medium'>#{index + 1}</span>
                    {result.success ? (
                      <span className='ml-2'>{result.loadTime.toFixed(2)} ms</span>
                    ) : (
                      <span className='ml-2'>❌ {result.error}</span>
                    )}
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>

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
