import { WebSettingsTest } from '@/components/WebSettingsTest'

export function HomePage() {
  return (
    <div className='flex h-full flex-col'>
      <div className='mt-auto mb-32 ml-16 text-2xl text-muted-foreground'>
        欢迎使用 InfinityMomo
      </div>
      
      {/* 临时添加的前端配置测试组件 */}
      <div className='p-4'>
        <WebSettingsTest />
      </div>
    </div>
  )
}

export default HomePage
