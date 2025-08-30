import { useTranslation } from '@/lib/i18n'

export function HomePage() {
  const { t } = useTranslation()

  return (
    <div className='flex h-full flex-col'>
      <div className='mt-auto mb-32 ml-16 text-2xl text-muted-foreground'>{t('home.welcome')}</div>
    </div>
  )
}

export default HomePage
