import './header.css';

export function Header() {
  // const location = useLocation()

  return (
    <header 
      className='h-12 bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60 px-4 flex items-center justify-between border-b drag-region'
    >
      <div className="flex-grow">
        {/* 空白区域，用于拖动 */}
      </div>
      <div className="no-drag-region">
        {/* 这里可以添加窗口控制按钮 */}
      </div>
    </header>
  )
}
