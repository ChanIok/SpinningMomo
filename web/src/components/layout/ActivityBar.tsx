import {
  Sidebar,
  SidebarContent,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
  SidebarGroup,
  SidebarGroupContent,
} from '@/components/ui/sidebar'
import { useNavigate, useLocation } from 'react-router'
import { Settings, Info, Home } from 'lucide-react'

interface MenuItem {
  key?: string
  title?: string
  icon?: React.ElementType
  type?: 'divider'
}

const menuItems: MenuItem[] = [
  { title: '主页', key: 'home', icon: Home },
  { title: '设置', key: 'settings', icon: Settings },
  { type: 'divider' },
  { title: '关于', key: 'about', icon: Info },
]

export function ActivityBar() {
  const navigate = useNavigate()
  const location = useLocation()
  const activeKey = location.pathname.split('/')[1] || 'home'

  const handleMenuSelect = (key: string) => {
    navigate(`/${key}`)
  }

  return (
    <Sidebar collapsible='none' className='pt-2 w-14 bg-transparent'>
      <SidebarContent>
        <SidebarGroup>
          <SidebarGroupContent className='px-0'>
            <SidebarMenu>
              {menuItems.map((item, index) =>
                item.type === 'divider' ? (
                  <div key={`divider-${index}`} className='mx-2 my-3'></div>
                ) : (
                  <SidebarMenuItem key={item.key}>
                    <SidebarMenuButton
                      tooltip={{
                        children: item.title,
                        hidden: false,
                      }}
                      isActive={activeKey === item.key}
                      onClick={() => item.key && handleMenuSelect(item.key)}
                      className='h-10 w-10 [&>svg]:mx-auto [&>svg]:h-5 [&>svg]:w-5'
                    >
                      {item.icon && <item.icon strokeWidth={1.8} />}
                    </SidebarMenuButton>
                  </SidebarMenuItem>
                )
              )}
            </SidebarMenu>
          </SidebarGroupContent>
        </SidebarGroup>
      </SidebarContent>
    </Sidebar>
  )
}
